/*
 * omap-abe-twl6040.c  --  SoC audio for TI OMAP based boards with ABE and
 *			   twl6040 codec
 *
 * Author: Misael Lopez Cruz <misael.lopez@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/mfd/twl6040.h>
#include <linux/platform_data/omap-abe-twl6040.h>
#include <linux/module.h>
#include <linux/i2c.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <sound/soc-dpcm.h>

#include <asm/mach-types.h>
#include <plat/hardware.h>
#include <plat/mux.h>

#include "omap-dmic.h"
#include "omap-mcpdm.h"
#include "omap-pcm.h"
#include "omap-abe-priv.h"
#include "mcbsp.h"
#include "omap-mcbsp.h"
#include "omap-dmic.h"
#include "../codecs/twl6040.h"

struct omap_abe_data {
	int twl6040_power_mode;
	int mcbsp_cfg;
	struct snd_soc_platform *abe_platform;
	struct i2c_client *tps6130x;
	struct i2c_adapter *adapter;
};

static struct i2c_board_info tps6130x_hwmon_info = {
	I2C_BOARD_INFO("tps6130x", 0x33),
};

/* configure the TPS6130x Handsfree Boost Converter */
static int omap_abe_tps6130x_configure(struct omap_abe_data *sdp4403)
{
	struct i2c_client *tps6130x = sdp4403->tps6130x;
	u8 data[2];

	data[0] = 0x01;
	data[1] = 0x60;
	if (i2c_master_send(tps6130x, data, 2) != 2)
		dev_err(&tps6130x->dev, "I2C write to TPS6130x failed\n");

	data[0] = 0x02;
	if (i2c_master_send(tps6130x, data, 2) != 2)
		dev_err(&tps6130x->dev, "I2C write to TPS6130x failed\n");
	return 0;
}

static int omap_abe_modem_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);
	struct snd_pcm_substream *modem_substream[2];
	struct snd_soc_pcm_runtime *modem_rtd;
	int channels, ret = 0, stream = substream->stream;

	modem_substream[stream] =
		snd_soc_get_dai_substream(card, OMAP_ABE_BE_MM_EXT1, stream);
	if (modem_substream[stream] == NULL)
		return -ENODEV;

	modem_rtd = modem_substream[stream]->private_data;

	if (!card_data->mcbsp_cfg) {
		/* Set cpu DAI configuration */
		ret = snd_soc_dai_set_fmt(modem_rtd->cpu_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM);
		if (ret < 0) {
			dev_err(card->dev, "can't set Modem cpu DAI configuration\n");
			return ret;
		} else
			card_data->mcbsp_cfg = 1;
	}

	if (params != NULL) {
		struct omap_mcbsp *mcbsp = snd_soc_dai_get_drvdata(
							modem_rtd->cpu_dai);
		/* Configure McBSP internal buffer usage */
		/* this need to be done for playback and/or record */
		channels = params_channels(params);
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_mcbsp_set_rx_threshold(mcbsp, channels);
		else
			omap_mcbsp_set_tx_threshold(mcbsp, channels);
	}

	return ret;
}

static int omap_abe_modem_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);

	card_data->mcbsp_cfg = 0;

	return 0;
}

static struct snd_soc_ops omap_abe_modem_ops = {
	.hw_params = omap_abe_modem_hw_params,
	.hw_free = omap_abe_modem_hw_free,
};

static int omap_abe_mcpdm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	struct omap_abe_twl6040_data *pdata = dev_get_platdata(card->dev);
	int clk_id, freq;
	int ret;

	clk_id = twl6040_get_clk_id(rtd->codec);
	if (clk_id == TWL6040_SYSCLK_SEL_HPPLL)
		freq = pdata->mclk_freq;
	else if (clk_id == TWL6040_SYSCLK_SEL_LPPLL)
		freq = 32768;
	else {
		dev_err(card->dev, "invalid clock\n");
		return -EINVAL;
	}

	/* set the codec mclk */
	ret = snd_soc_dai_set_sysclk(codec_dai, clk_id, freq,
				SND_SOC_CLOCK_IN);
	if (ret)
		dev_err(card->dev, "can't set codec system clock\n");

	return ret;
}

static struct snd_soc_ops omap_abe_mcpdm_ops = {
	.hw_params = omap_abe_mcpdm_hw_params,
};

static int omap_abe_mcbsp_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	int ret;
	unsigned int be_id, channels;

	be_id = rtd->dai_link->be_id;

	if (be_id == OMAP_ABE_DAI_BT_VX)
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_B |
				SND_SOC_DAIFMT_NB_IF | SND_SOC_DAIFMT_CBM_CFM);
	else
		/* Set cpu DAI configuration */
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
				SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);

	if (ret < 0) {
		dev_err(card->dev, "can't set cpu DAI configuration\n");
		return ret;
	}

	if (params != NULL) {
		struct omap_mcbsp *mcbsp = snd_soc_dai_get_drvdata(cpu_dai);
		/* Configure McBSP internal buffer usage */
		/* this need to be done for playback and/or record */
		channels = params_channels(params);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_mcbsp_set_tx_threshold(mcbsp, channels);
		else
			omap_mcbsp_set_rx_threshold(mcbsp, channels);
	}

	/* Set McBSP clock to external */
	ret = snd_soc_dai_set_sysclk(cpu_dai, OMAP_MCBSP_SYSCLK_CLKS_FCLK,
				     64 * params_rate(params), SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "can't set cpu system clock\n");

	return ret;
}

static struct snd_soc_ops omap_abe_mcbsp_ops = {
	.hw_params = omap_abe_mcbsp_hw_params,
};

static int omap_abe_dmic_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	int ret = 0;

	ret = snd_soc_dai_set_sysclk(cpu_dai, OMAP_DMIC_SYSCLK_PAD_CLKS,
				     19200000, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(card->dev, "can't set DMIC in system clock\n");
		return ret;
	}
	ret = snd_soc_dai_set_sysclk(cpu_dai, OMAP_DMIC_ABE_DMIC_CLK, 2400000,
				     SND_SOC_CLOCK_OUT);
	if (ret < 0)
		dev_err(card->dev, "can't set DMIC output clock\n");

	return ret;
}

static struct snd_soc_ops omap_abe_dmic_ops = {
	.hw_params = omap_abe_dmic_hw_params,
};

static int mcbsp_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *channels = hw_param_interval(params,
                                       SNDRV_PCM_HW_PARAM_CHANNELS);
	unsigned int be_id = rtd->dai_link->be_id;

	if (be_id == OMAP_ABE_DAI_MM_FM || be_id == OMAP_ABE_DAI_BT_VX)
		channels->min = 2;

	snd_mask_set(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
	                            SNDRV_PCM_HW_PARAM_FIRST_MASK],
	                            SNDRV_PCM_FORMAT_S16_LE);
	return 0;
}

static int dmic_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);

	/* The ABE will covert the FE rate to 96k */
	rate->min = rate->max = 96000;

	snd_mask_set(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
	                            SNDRV_PCM_HW_PARAM_FIRST_MASK],
	                            SNDRV_PCM_FORMAT_S32_LE);
	return 0;
}
/* Headset jack */
static struct snd_soc_jack hs_jack;

/*Headset jack detection DAPM pins */
static struct snd_soc_jack_pin hs_jack_pins[] = {
	{
		.pin = "Headset Mic",
		.mask = SND_JACK_MICROPHONE,
	},
	{
		.pin = "Headset Stereophone",
		.mask = SND_JACK_HEADPHONE,
	},
};

#if 0
static int omap_abe_get_power_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = card_data->twl6040_power_mode;
	return 0;
}

static int omap_abe_set_power_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);

	if (card_data->twl6040_power_mode == ucontrol->value.integer.value[0])
		return 0;

	card_data->twl6040_power_mode = ucontrol->value.integer.value[0];
	omap_abe_pm_set_mode(card_data->abe_platform,
			card_data->twl6040_power_mode);

	return 1;
}

static const char *power_texts[] = {"Low-Power", "High-Performance"};

static const struct soc_enum omap_abe_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, power_texts),
};

static const struct snd_kcontrol_new omap_abe_controls[] = {
	SOC_ENUM_EXT("TWL6040 Power Mode", omap_abe_enum[0],
		omap_abe_get_power_mode, omap_abe_set_power_mode),
};
#endif

/* OMAP ABE TWL6040 machine DAPM */
static const struct snd_soc_dapm_widget twl6040_dapm_widgets[] = {
	/* Outputs */
	SND_SOC_DAPM_HP("Headset Stereophone", NULL),
	SND_SOC_DAPM_SPK("Earphone Spk", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),
	SND_SOC_DAPM_SPK("Vibrator", NULL),

	/* Inputs */
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Handset Mic", NULL),
	SND_SOC_DAPM_LINE("Line In", NULL),
	SND_SOC_DAPM_MIC("Digital Mic 0", NULL),
	SND_SOC_DAPM_MIC("Digital Mic 1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic 2", NULL),

	/* MODEM */
	SND_SOC_DAPM_INPUT("MODEM IN"),
	SND_SOC_DAPM_OUTPUT("MODEM OUT"),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* Routings for outputs */
	{"Headset Stereophone", NULL, "HSOL"},
	{"Headset Stereophone", NULL, "HSOR"},

	{"Earphone Spk", NULL, "EP"},

	{"Ext Spk", NULL, "HFL"},
	{"Ext Spk", NULL, "HFR"},

	{"Line Out", NULL, "AUXL"},
	{"Line Out", NULL, "AUXR"},

	{"Vibrator", NULL, "VIBRAL"},
	{"Vibrator", NULL, "VIBRAR"},

	/* Routings for inputs */
	{"HSMIC", NULL, "Headset Mic"},
	{"Headset Mic", NULL, "Headset Mic Bias"},

	{"MAINMIC", NULL, "Main Handset Mic"},
	{"Main Handset Mic", NULL, "Main Mic Bias"},

	{"SUBMIC", NULL, "Sub Handset Mic"},
	{"Sub Handset Mic", NULL, "Main Mic Bias"},

	{"AFML", NULL, "Line In"},
	{"AFMR", NULL, "Line In"},

	/* Digital Mics: DMic0, DMic1, DMic2 with bias */
	{"DMIC0", NULL, "Digital Mic1 Bias"},
	{"Digital Mic1 Bias", NULL, "Digital Mic 0"},

	{"DMIC1", NULL, "Digital Mic1 Bias"},
	{"Digital Mic1 Bias", NULL, "Digital Mic 1"},

	{"DMIC2", NULL, "Digital Mic1 Bias"},
	{"Digital Mic1 Bias", NULL, "Digital Mic 2"},

	/* Connections between twl6040 and ABE */
	{"Headset Playback", NULL, "PDM_DL1"},
	{"Handsfree Playback", NULL, "PDM_DL2"},
	{"Vibra Playback", NULL, "PDM_VIB"},
	{"PDM_UL1", NULL, "PDM Capture"},

	/* Bluetooth <--> ABE*/
	{"omap-mcbsp.1 Playback", NULL, "BT_VX_DL"},
	{"BT_VX_UL", NULL, "omap-mcbsp.1 Capture"},

	/* FM <--> ABE */
	{"omap-mcbsp.2 Playback", NULL, "MM_EXT_DL"},
	{"MM_EXT_UL", NULL, "omap-mcbsp.2 Capture"},

	/* MODEM <--> ABE */
	{"Voice Playback", NULL, "MODEM OUT"},
	{"MODEM IN", NULL, "Voice Capture"},
};

static inline void twl6040_disconnect_pin(struct snd_soc_dapm_context *dapm,
					  int connected, char *pin)
{
	if (!connected)
		snd_soc_dapm_disable_pin(dapm, pin);
}

#if 0
/* set optimum DL1 output path gain for ABE -> McPDM -> twl6040 */
static int omap_abe_set_pdm_dl1_gains(struct snd_soc_dapm_context *dapm)
{
	int gain_dB, gain;

	gain_dB = twl6040_get_dl1_gain(dapm->codec);

	switch (gain_dB) {
	case -8:
		gain = GAIN_M8dB;
		break;
	case -1:
		gain = GAIN_M1dB;
		break;
	default:
		gain = GAIN_0dB;
		break;
	}

	abe_write_gain(GAINS_DL1, gain, RAMP_2MS, GAIN_LEFT_OFFSET);
	abe_write_gain(GAINS_DL1, gain, RAMP_2MS, GAIN_RIGHT_OFFSET);
	return 0;
}
#endif

static int omap_abe_twl6040_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct snd_soc_platform *platform = rtd->platform;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct omap_abe_twl6040_data *pdata = dev_get_platdata(card->dev);
	u32 hsotrim, left_offset, right_offset, step_mV;
	int ret = 0;

	/* Disable not connected paths if not used */
	twl6040_disconnect_pin(dapm, pdata->has_hs, "Headset Stereophone");
	twl6040_disconnect_pin(dapm, pdata->has_hf, "Ext Spk");
	twl6040_disconnect_pin(dapm, pdata->has_ep, "Earphone Spk");
	twl6040_disconnect_pin(dapm, pdata->has_aux, "Line Out");
	twl6040_disconnect_pin(dapm, pdata->has_vibra, "Vibrator");
	twl6040_disconnect_pin(dapm, pdata->has_hsmic, "Headset Mic");
	twl6040_disconnect_pin(dapm, pdata->has_mainmic, "Main Handset Mic");
	twl6040_disconnect_pin(dapm, pdata->has_submic, "Sub Handset Mic");
	twl6040_disconnect_pin(dapm, pdata->has_afm, "Line In");

	/* allow audio paths from the audio modem to run during suspend */
	snd_soc_dapm_ignore_suspend(dapm, "Ext Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Ext Spk");
	snd_soc_dapm_ignore_suspend(dapm, "AFML");
	snd_soc_dapm_ignore_suspend(dapm, "AFMR");
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Headset Stereophone");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic 0");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic 1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic 2");

	/* DC offset cancellation computation only if ABE is enabled */
	if (pdata->has_abe) {
		hsotrim = twl6040_get_trim_value(codec, TWL6040_TRIM_HSOTRIM);
		right_offset = TWL6040_HSF_TRIM_RIGHT(hsotrim);
		left_offset = TWL6040_HSF_TRIM_LEFT(hsotrim);

		step_mV = twl6040_get_hs_step_size(codec);
		omap_abe_dc_set_hs_offset(platform, left_offset, right_offset,
					  step_mV);
	}

	/* Headset jack detection only if it is supported */
	if (pdata->jack_detection) {
		ret = snd_soc_jack_new(codec, "Headset Jack",
					SND_JACK_HEADSET, &hs_jack);
		if (ret)
			return ret;

		ret = snd_soc_jack_add_pins(&hs_jack, ARRAY_SIZE(hs_jack_pins),
					hs_jack_pins);
		if (machine_is_omap_4430sdp())
			twl6040_hs_jack_detect(codec, &hs_jack, SND_JACK_HEADSET);
		else
			snd_soc_jack_report(&hs_jack, SND_JACK_HEADSET, SND_JACK_HEADSET);
	}

	return ret;
}

static const struct snd_soc_dapm_widget dmic_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Digital Mic", NULL),
};

static const struct snd_soc_dapm_route dmic_audio_map[] = {
	{"DMic", NULL, "Digital Mic"},
	{"Digital Mic", NULL, "Digital Mic1 Bias"},
};

static int omap_abe_dmic_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret;

	ret = snd_soc_dapm_new_controls(dapm, dmic_dapm_widgets,
				ARRAY_SIZE(dmic_dapm_widgets));
	if (ret)
		return ret;

	return snd_soc_dapm_add_routes(dapm, dmic_audio_map,
				ARRAY_SIZE(dmic_audio_map));
}

static int omap_abe_twl6040_dl2_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_platform *platform = rtd->platform;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct omap_abe_twl6040_data *pdata = dev_get_platdata(card->dev);
	u32 hfotrim, left_offset, right_offset;

	/* DC offset cancellation computation only if ABE is enabled */
	if (pdata->has_abe) {
		/* DC offset cancellation computation */
		hfotrim = twl6040_get_trim_value(codec, TWL6040_TRIM_HFOTRIM);
		right_offset = TWL6040_HSF_TRIM_RIGHT(hfotrim);
		left_offset = TWL6040_HSF_TRIM_LEFT(hfotrim);

		omap_abe_dc_set_hf_offset(platform, left_offset, right_offset);
	}

	return 0;
}

static int omap_abe_twl6040_fe_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_platform *platform = rtd->platform;
	struct snd_soc_card *card = rtd->card;
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);

	card_data->abe_platform = platform;

	return 0;
}

#if 0
static int omap_abe_stream_event(struct snd_soc_dapm_context *dapm)
{
	/*
	 * set DL1 gains dynamically according to the active output
	 * (Headset, Earpiece) and HSDAC power mode
	 */
	return omap_abe_set_pdm_dl1_gains(dapm);
}
#endif

/* Digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link twl6040_dmic_dai[] = {
	{
		.name = "TWL6040",
		.stream_name = "TWL6040",
		.cpu_dai_name = "omap-mcpdm",
		.codec_dai_name = "twl6040-legacy",
		.platform_name = "omap-pcm-audio",
		.codec_name = "twl6040-codec",
		.init = omap_abe_twl6040_init,
		.ops = &omap_abe_mcpdm_ops,
	},
	{
		.name = "DMIC",
		.stream_name = "DMIC Capture",
		.cpu_dai_name = "omap-dmic",
		.codec_dai_name = "dmic-hifi",
		.platform_name = "omap-pcm-audio",
		.codec_name = "dmic-codec",
		.init = omap_abe_dmic_init,
		.ops = &omap_abe_dmic_ops,
	},
};

static struct snd_soc_dai_link twl6040_only_dai[] = {
	{
		.name = "TWL6040",
		.stream_name = "TWL6040",
		.cpu_dai_name = "omap-mcpdm",
		.codec_dai_name = "twl6040-legacy",
		.platform_name = "omap-pcm-audio",
		.codec_name = "twl6040-codec",
		.init = omap_abe_twl6040_init,
		.ops = &omap_abe_mcpdm_ops,
	},
};


static struct snd_soc_dai_link omap_abe_dai[] = {

/*
 * Frontend DAIs - i.e. userspace visible interfaces (ALSA PCMs)
 */

	{
		.name = "OMAP ABE Media",
		.stream_name = "Multimedia",

		/* ABE components - MM-UL & MM_DL */
		.cpu_dai_name = "MultiMedia1",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.ignore_pmdown_time = 1, /* Power down without delay */
		.init = omap_abe_twl6040_fe_init,
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{
		.name = "OMAP ABE Media Capture",
		.stream_name = "Multimedia Capture",

		/* ABE components - MM-UL2 */
		.cpu_dai_name = "MultiMedia2",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{
		.name = "OMAP ABE Voice",
		.stream_name = "Voice",

		/* ABE components - VX-UL & VX-DL */
		.cpu_dai_name = "Voice",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
		.no_host_mode = SND_SOC_DAI_LINK_OPT_HOST,
	},
	{
		.name = "OMAP ABE Tones Playback",
		.stream_name = "Tone Playback",

		/* ABE components - TONES_DL */
		.cpu_dai_name = "Tones",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{
		.name = "OMAP ABE Vibra Playback",
		.stream_name = "VIB-DL",

		/* ABE components - DMIC UL 2 */
		.cpu_dai_name = "Vibra",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{
		.name = "OMAP ABE MODEM",
		.stream_name = "MODEM",

		/* ABE components - MODEM <-> McBSP2 */
		.cpu_dai_name = "MODEM",
		.platform_name = "aess",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.ignore_pmdown_time = 1, /* Power down without delay */
		.init = omap_abe_twl6040_fe_init,
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
		.ops = &omap_abe_modem_ops,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
	},
	{
		.name = "OMAP ABE Media LP",
		.stream_name = "Multimedia",

		/* ABE components - MM-DL (mmap) */
		.cpu_dai_name = "MultiMedia1 LP",
		.platform_name = "aess",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{
		.name = "Legacy McBSP",
		.stream_name = "Multimedia",

		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp.2",
		.platform_name = "omap-pcm-audio",

		/* FM */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.ops = &omap_abe_mcbsp_ops,
		.ignore_suspend = 1,
	},
	{
		.name = "Legacy McPDM",
		.stream_name = "Headset Playback",

		/* ABE components - DL1 */
		.cpu_dai_name = "mcpdm-legacy",
		.platform_name = "omap-pcm-audio",

		/* Phoenix - DL1 DAC */
		.codec_dai_name =  "twl6040-legacy",
		.codec_name = "twl6040-codec",

		.ops = &omap_abe_mcpdm_ops,
		.ignore_suspend = 1,
	},
#if 0
	{
		.name = "SPDIF",
		.stream_name = "SPDIF",
		.cpu_dai_name = "omap-mcasp-dai.0",
		.codec_dai_name = "dit-hifi",	/* dummy s/pdif transciever
						 * driver */
		.platform_name = "omap-pcm-audio",
		.ignore_suspend = 1,
	},
#endif
	{
		.name = "Legacy DMIC",
		.stream_name = "DMIC Capture",

		/* ABE components - DMIC0 */
		.cpu_dai_name = "omap-dmic",
		.platform_name = "omap-pcm-audio",

		/* DMIC codec */
		.codec_dai_name = "dmic-hifi",
		.codec_name = "dmic-codec",

		.init = omap_abe_dmic_init,
		.ops = &omap_abe_dmic_ops,
	},

/*
 * Backend DAIs - i.e. dynamically matched interfaces, invisible to userspace.
 * Matched to above interfaces at runtime, based upon use case.
 */

	{
		.name = OMAP_ABE_BE_PDM_DL1,
		.stream_name = "HS Playback",

		/* ABE components - DL1 */
		.cpu_dai_name = "mcpdm-dl1",
		.platform_name = "aess",

		/* Phoenix - DL1 DAC */
		.codec_dai_name =  "twl6040-dl1",
		.codec_name = "twl6040-codec",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.ignore_pmdown_time = 1, /* Power down without delay */
		.init = omap_abe_twl6040_init,
		.ops = &omap_abe_mcpdm_ops,
		.be_id = OMAP_ABE_DAI_PDM_DL1,
		.ignore_suspend = 1,
	},
	{
		.name = OMAP_ABE_BE_PDM_UL1,
		.stream_name = "Analog Capture",

		/* ABE components - UL1 */
		.cpu_dai_name = "mcpdm-ul1",
		.platform_name = "aess",

		/* Phoenix - UL ADC */
		.codec_dai_name =  "twl6040-ul",
		.codec_name = "twl6040-codec",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.ops = &omap_abe_mcpdm_ops,
		.be_id = OMAP_ABE_DAI_PDM_UL,
		.ignore_suspend = 1,
	},
	{
		.name = OMAP_ABE_BE_PDM_DL2,
		.stream_name = "HF Playback",

		/* ABE components - DL2 */
		.cpu_dai_name = "mcpdm-dl2",
		.platform_name = "aess",

		/* Phoenix - DL2 DAC */
		.codec_dai_name =  "twl6040-dl2",
		.codec_name = "twl6040-codec",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.init = omap_abe_twl6040_dl2_init,
		.ops = &omap_abe_mcpdm_ops,
		.be_id = OMAP_ABE_DAI_PDM_DL2,
		.ignore_suspend = 1,
	},
	{
		.name = OMAP_ABE_BE_PDM_VIB,
		.stream_name = "Vibra",

		/* ABE components - VIB1 DL */
		.cpu_dai_name = "mcpdm-vib",
		.platform_name = "aess",

		/* Phoenix - PDM to PWM */
		.codec_dai_name =  "twl6040-vib",
		.codec_name = "twl6040-codec",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.ops = &omap_abe_mcpdm_ops,
		.be_id = OMAP_ABE_DAI_PDM_VIB,
	},
	{
		.name = OMAP_ABE_BE_BT_VX_UL,
		.stream_name = "BT Capture",

		/* ABE components - MCBSP1 - BT-VX */
		.cpu_dai_name = "omap-mcbsp.1",
		.platform_name = "aess",

		/* Bluetooth */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &omap_abe_mcbsp_ops,
		.be_id = OMAP_ABE_DAI_BT_VX,
		.ignore_suspend = 1,
	},
	{
		.name = OMAP_ABE_BE_BT_VX_DL,
		.stream_name = "BT Playback",

		/* ABE components - MCBSP1 - BT-VX */
		.cpu_dai_name = "omap-mcbsp.1",
		.platform_name = "aess",

		/* Bluetooth */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.ignore_pmdown_time = 1, /* Power down without delay */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &omap_abe_mcbsp_ops,
		.be_id = OMAP_ABE_DAI_BT_VX,
		.ignore_suspend = 1,
	},
	{
		.name = OMAP_ABE_BE_MM_EXT0,
		.stream_name = "FM",

		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp.2",
		.platform_name = "aess",

		/* FM */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &omap_abe_mcbsp_ops,
		.be_id = OMAP_ABE_DAI_MM_FM,
	},
#if 0
	{
		.name = OMAP_ABE_BE_MM_EXT1,
		.stream_name = "MODEM",

		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp.2",
		.platform_name = "aess",

		/* MODEM */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &omap_abe_mcbsp_ops,
		.be_id = OMAP_ABE_DAI_MODEM,
		.ignore_suspend = 1,
	},
#endif
	{
		.name = OMAP_ABE_BE_DMIC0,
		.stream_name = "DMIC0 Capture",

		/* ABE components - DMIC UL 1 */
		.cpu_dai_name = "omap-dmic-abe-dai-0",
		.platform_name = "aess",

		/* DMIC 0 */
		.codec_dai_name = "dmic-hifi",
		.codec_name = "dmic-codec",
		.ops = &omap_abe_dmic_ops,

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = dmic_be_hw_params_fixup,
		.be_id = OMAP_ABE_DAI_DMIC0,
	},
	{
		.name = OMAP_ABE_BE_DMIC1,
		.stream_name = "DMIC1 Capture",

		/* ABE components - DMIC UL 1 */
		.cpu_dai_name = "omap-dmic-abe-dai-1",
		.platform_name = "aess",

		/* DMIC 1 */
		.codec_dai_name = "dmic-hifi",
		.codec_name = "dmic-codec",
		.ops = &omap_abe_dmic_ops,

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = dmic_be_hw_params_fixup,
		.be_id = OMAP_ABE_DAI_DMIC1,
	},
	{
		.name = OMAP_ABE_BE_DMIC2,
		.stream_name = "DMIC2 Capture",

		/* ABE components - DMIC UL 2 */
		.cpu_dai_name = "omap-dmic-abe-dai-2",
		.platform_name = "aess",

		/* DMIC 2 */
		.codec_dai_name = "dmic-hifi",
		.codec_name = "dmic-codec",
		.ops = &omap_abe_dmic_ops,

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = dmic_be_hw_params_fixup,
		.be_id = OMAP_ABE_DAI_DMIC2,
	},
};

static struct snd_soc_dai_link omap_abe_no_dmic_dai[] = {

/*
 * Frontend DAIs - i.e. userspace visible interfaces (ALSA PCMs)
 */

	{
		.name = "OMAP ABE Media",
		.stream_name = "Multimedia",

		/* ABE components - MM-UL & MM_DL */
		.cpu_dai_name = "MultiMedia1",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.ignore_pmdown_time = 1, /* Power down without delay */
		.init = omap_abe_twl6040_fe_init,
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{
		.name = "OMAP ABE Media Capture",
		.stream_name = "Multimedia Capture",

		/* ABE components - MM-UL2 */
		.cpu_dai_name = "MultiMedia2",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{
		.name = "OMAP ABE Voice",
		.stream_name = "Voice",

		/* ABE components - VX-UL & VX-DL */
		.cpu_dai_name = "Voice",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
		.no_host_mode = SND_SOC_DAI_LINK_OPT_HOST,
	},
	{
		.name = "OMAP ABE Tones Playback",
		.stream_name = "Tone Playback",

		/* ABE components - TONES_DL */
		.cpu_dai_name = "Tones",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{
		.name = "OMAP ABE Vibra Playback",
		.stream_name = "VIB-DL",

		/* ABE components - Vibra */
		.cpu_dai_name = "Vibra",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{
		.name = "OMAP ABE MODEM",
		.stream_name = "MODEM",

		/* ABE components - MODEM <-> McBSP2 */
		.cpu_dai_name = "MODEM",
		.platform_name = "aess",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.ignore_pmdown_time = 1, /* Power down without delay */
		.init = omap_abe_twl6040_fe_init,
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
		.ops = &omap_abe_modem_ops,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
	},
	{
		.name = "OMAP ABE Media LP",
		.stream_name = "Multimedia",

		/* ABE components - MM-DL (mmap) */
		.cpu_dai_name = "MultiMedia1 LP",
		.platform_name = "aess",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.dynamic = 1, /* BE is dynamic */
		.trigger = {SND_SOC_DPCM_TRIGGER_BESPOKE, SND_SOC_DPCM_TRIGGER_BESPOKE},
	},
	{
		.name = "Legacy McBSP",
		.stream_name = "Multimedia",

		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp.2",
		.platform_name = "omap-pcm-audio",

		/* FM */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.ops = &omap_abe_mcbsp_ops,
		.ignore_suspend = 1,
	},
	{
		.name = "Legacy McPDM",
		.stream_name = "Headset Playback",

		/* ABE components - DL1 */
		.cpu_dai_name = "mcpdm-legacy",
		.platform_name = "omap-pcm-audio",

		/* Phoenix - DL1 DAC */
		.codec_dai_name =  "twl6040-legacy",
		.codec_name = "twl6040-codec",

		.ops = &omap_abe_mcpdm_ops,
		.ignore_suspend = 1,
	},
/*
 * Backend DAIs - i.e. dynamically matched interfaces, invisible to userspace.
 * Matched to above interfaces at runtime, based upon use case.
 */

	{
		.name = OMAP_ABE_BE_PDM_DL1,
		.stream_name = "HS Playback",

		/* ABE components - DL1 */
		.cpu_dai_name = "mcpdm-dl1",
		.platform_name = "aess",

		/* Phoenix - DL1 DAC */
		.codec_dai_name =  "twl6040-dl1",
		.codec_name = "twl6040-codec",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.ignore_pmdown_time = 1, /* Power down without delay */
		.init = omap_abe_twl6040_init,
		.ops = &omap_abe_mcpdm_ops,
		.be_id = OMAP_ABE_DAI_PDM_DL1,
		.ignore_suspend = 1,
	},
	{
		.name = OMAP_ABE_BE_PDM_UL1,
		.stream_name = "Analog Capture",

		/* ABE components - UL1 */
		.cpu_dai_name = "mcpdm-ul1",
		.platform_name = "aess",

		/* Phoenix - UL ADC */
		.codec_dai_name =  "twl6040-ul",
		.codec_name = "twl6040-codec",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.ops = &omap_abe_mcpdm_ops,
		.be_id = OMAP_ABE_DAI_PDM_UL,
		.ignore_suspend = 1,
	},
	{
		.name = OMAP_ABE_BE_PDM_DL2,
		.stream_name = "HF Playback",

		/* ABE components - DL2 */
		.cpu_dai_name = "mcpdm-dl2",
		.platform_name = "aess",

		/* Phoenix - DL2 DAC */
		.codec_dai_name =  "twl6040-dl2",
		.codec_name = "twl6040-codec",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.init = omap_abe_twl6040_dl2_init,
		.ops = &omap_abe_mcpdm_ops,
		.be_id = OMAP_ABE_DAI_PDM_DL2,
		.ignore_suspend = 1,
	},
	{
		.name = OMAP_ABE_BE_PDM_VIB,
		.stream_name = "Vibra",

		/* ABE components - VIB1 DL */
		.cpu_dai_name = "mcpdm-vib",
		.platform_name = "aess",

		/* Phoenix - PDM to PWM */
		.codec_dai_name =  "twl6040-vib",
		.codec_name = "twl6040-codec",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.ops = &omap_abe_mcpdm_ops,
		.be_id = OMAP_ABE_DAI_PDM_VIB,
	},
	{
		.name = OMAP_ABE_BE_BT_VX_UL,
		.stream_name = "BT Capture",

		/* ABE components - MCBSP1 - BT-VX */
		.cpu_dai_name = "omap-mcbsp.1",
		.platform_name = "aess",

		/* Bluetooth */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &omap_abe_mcbsp_ops,
		.be_id = OMAP_ABE_DAI_BT_VX,
		.ignore_suspend = 1,
	},
	{
		.name = OMAP_ABE_BE_BT_VX_DL,
		.stream_name = "BT Playback",

		/* ABE components - MCBSP1 - BT-VX */
		.cpu_dai_name = "omap-mcbsp.1",
		.platform_name = "aess",

		/* Bluetooth */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.ignore_pmdown_time = 1, /* Power down without delay */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &omap_abe_mcbsp_ops,
		.be_id = OMAP_ABE_DAI_BT_VX,
		.ignore_suspend = 1,
	},
	{
		.name = OMAP_ABE_BE_MM_EXT0,
		.stream_name = "FM",

		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp.2",
		.platform_name = "aess",

		/* FM */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,
		.ops = &omap_abe_mcbsp_ops,
		.be_id = OMAP_ABE_DAI_MM_FM,
	},
};

/* Audio machine driver */
static struct snd_soc_card omap_abe_card = {
	.owner = THIS_MODULE,

	.dapm_widgets = twl6040_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(twl6040_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),

};

static __devinit int omap_abe_probe(struct platform_device *pdev)
{
	struct omap_abe_twl6040_data *pdata = dev_get_platdata(&pdev->dev);
	struct snd_soc_card *card = &omap_abe_card;
	struct omap_abe_data *card_data;
	int ret;

	card->dev = &pdev->dev;

	if (!pdata) {
		dev_err(&pdev->dev, "Missing pdata\n");
		return -ENODEV;
	}

	if (pdata->card_name) {
		card->name = pdata->card_name;
	} else {
		dev_err(&pdev->dev, "Card name is not provided\n");
		return -ENODEV;
	}

	if (!pdata->mclk_freq) {
		dev_err(&pdev->dev, "MCLK frequency missing\n");
		return -ENODEV;
	}

	if (pdata->has_abe) {
		if (pdata->has_dmic) {
			card->dai_link = omap_abe_dai;
			card->num_links = ARRAY_SIZE(omap_abe_dai);
		} else {
			card->dai_link = omap_abe_no_dmic_dai;
			card->num_links = ARRAY_SIZE(omap_abe_no_dmic_dai);
		}
	} else if (pdata->has_dmic) {
		card->dai_link = twl6040_dmic_dai;
		card->num_links = ARRAY_SIZE(twl6040_dmic_dai);
	} else {
		card->dai_link = twl6040_only_dai;
		card->num_links = ARRAY_SIZE(twl6040_only_dai);
	}

	card_data = devm_kzalloc(&pdev->dev, sizeof(*card_data), GFP_KERNEL);
	if (card_data == NULL)
		return -ENOMEM;
	snd_soc_card_set_drvdata(card, card_data);

	/* Only configure the TPS6130x on SDP4430 */
	if (machine_is_omap_4430sdp()) {
		card_data->adapter = i2c_get_adapter(1);
		if (!card_data->adapter) {
			dev_err(&pdev->dev, "can't get i2c adapter\n");
			return -ENODEV;
		}

		card_data->tps6130x = i2c_new_device(card_data->adapter,
				&tps6130x_hwmon_info);
		if (!card_data->tps6130x) {
			dev_err(&pdev->dev, "can't add i2c device\n");
			ret = -ENODEV;
			goto err_i2c;
		}

		omap_abe_tps6130x_configure(card_data);
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
		goto err_card;
	}

	return 0;

err_card:
	i2c_unregister_device(card_data->tps6130x);
err_i2c:
	i2c_put_adapter(card_data->adapter);
	return ret;
}

static int __devexit omap_abe_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct omap_abe_data *card_data = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);
	i2c_unregister_device(card_data->tps6130x);
	i2c_put_adapter(card_data->adapter);

	return 0;
}

static struct platform_driver omap_abe_driver = {
	.driver = {
		.name = "omap-abe-twl6040",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = omap_abe_probe,
	.remove = __devexit_p(omap_abe_remove),
};

module_platform_driver(omap_abe_driver);

MODULE_AUTHOR("Misael Lopez Cruz <misael.lopez@ti.com>");
MODULE_DESCRIPTION("ALSA SoC for OMAP boards with ABE and twl6040 codec");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:omap-abe-twl6040");
