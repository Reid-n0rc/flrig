// ----------------------------------------------------------------------------
// Copyright (C) 2026
//              David Freese, W1HKJ
//
// This file is part of flrig.
//
// flrig is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// flrig is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include "yaesu/FTX_1.h"

#include <sstream>

#include "debug.h"
#include "support.h"
#include "trace.h"

#define FTX_1_WAIT_TIME 200

// mode index
enum mFTX_1 {
	mLSB, mUSB, mCW_U, mFM, mAM, mRTTYL, mCW_L, mDATAL, mRTTYU,
	mDATAFM, mFMN, mDATAU, mAMN, mPSK, mDATAFMN, mC4FMDN, mC4FMVW };

static const char FTX_1_NAME[] = "FTX-1";

static std::vector<std::string> ftx1_modes;
static const char *FTX_1_MODE_NAMES[] = {
	"LSB",     "USB",     "CW-U",    "FM",      "AM",
	"RTTY-L",  "CW-L",    "DATA-L",  "RTTY-U",  "DATA-FM",
	"FM-N",    "DATA-U",  "AM-N",    "PSK",     "DATA-FM-N",
	"C4FM-DN", "C4FM-VW" };

// MD P2 mode character for each mode index
static const char FTX_1_MODE_CHR[] = {
	'1', '2', '3', '4', '5',
	'6', '7', '8', '9', 'A',
	'B', 'C', 'D', 'E', 'F',
	'H', 'I' };

static const char FTX_1_MODE_TYPE[] = {
	'L', 'U', 'U', 'U', 'U',
	'L', 'L', 'L', 'U', 'U',
	'U', 'U', 'U', 'U', 'U',
	'U', 'U' };

// default bandwidth index for each mode index
static const int FTX_1_DEF_BW[] = {
	17, 17,  5,  0,  0,
	10,  5, 16, 10,  0,
	 0, 16,  0, 16,  0,
	 0,  0 };

// SH widths, CAT manual Table 5
static std::vector<std::string> ftx1_widths_ssb;
static const char *FTX_1_WIDTHS_SSB[] = {
	 "300",  "400",  "600",  "850", "1100",
	"1200", "1500", "1650", "1800", "1950",
	"2100", "2250", "2400", "2450", "2500",
	"2600", "2700", "2800", "2900", "3000",
	"3200", "3500", "4000" };
static const int FTX_1_WVALS_SSB[] = {
	 1,  2,  3,  4,  5,
	 6,  7,  8,  9, 10,
	11, 12, 13, 14, 15,
	16, 17, 18, 19, 20,
	21, 22, 23, WVALS_LIMIT };

// DATA and PSK
static std::vector<std::string> ftx1_widths_data;
static const char *FTX_1_WIDTHS_DATA[] = {
	  "50",  "100",  "150",  "200",  "250",
	 "300",  "350",  "400",  "450",  "500",
	 "600",  "800", "1200", "1400", "1700",
	"2000", "2400", "3000", "3200", "3500",
	"4000" };
static const int FTX_1_WVALS_DATA[] = {
	 1,  2,  3,  4,  5,
	 6,  7,  8,  9, 10,
	11, 12, 13, 14, 15,
	16, 17, 18, 19, 20,
	21, WVALS_LIMIT };

// CW and RTTY
static std::vector<std::string> ftx1_widths_cw;
static const char *FTX_1_WIDTHS_CW[] = {
	  "50",  "100",  "150",  "200",  "250",
	 "300",  "350",  "400",  "450",  "500",
	 "600",  "800", "1200", "1400", "1700",
	"2000", "2400", "3000", "3200", "3500",
	"4000" };
static const int FTX_1_WVALS_CW[] = {
	 1,  2,  3,  4,  5,
	 6,  7,  8,  9, 10,
	11, 12, 13, 14, 15,
	16, 17, 18, 19, 20,
	21, WVALS_LIMIT };

// Single bandwidth modes
// AM, AM-N
static std::vector<std::string> ftx1_widths_am;
static const char *FTX_1_WIDTHS_AM[] = { "6000" };
static const int FTX_1_WVALS_AM[] = { 0, WVALS_LIMIT };

// FM-N, DATA-FM-N
static std::vector<std::string> ftx1_widths_fmn;
static const char *FTX_1_WIDTHS_FMN[] = { "9000" };
static const int FTX_1_WVALS_FMN[] = { 0, WVALS_LIMIT };

// FM, DATA-FM, C4FM
static std::vector<std::string> ftx1_widths_fm;
static const char *FTX_1_WIDTHS_FM[] = { "16000" };
static const int FTX_1_WVALS_FM[] = { 0, WVALS_LIMIT };

// SD (CW break-in delay) and VD (VOX delay) codes 00 - 33, in msec
static const int FTX_1_DELAY_MSEC[] = {
	  30,   50,  100,  150,  200,  250,  300,  400,  500,  600,
	 700,  800,  900, 1000, 1100, 1200, 1300, 1400, 1500, 1600,
	1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600,
	2700, 2800, 2900, 3000 };
static const int FTX_1_DELAY_CODES =
	sizeof(FTX_1_DELAY_MSEC) / sizeof(*FTX_1_DELAY_MSEC);

/// SD / VD code closest to msec
static int delay_code(int msec) {
	int best = 0;
	for (int code = 1; code < FTX_1_DELAY_CODES; code++) {
		if (abs(FTX_1_DELAY_MSEC[code] - msec) <
			abs(FTX_1_DELAY_MSEC[best] - msec)) {
			best = code;
		}
	}
	return best;
}

/// msec for an SD / VD code, or -1 if the code is out of range
static int delay_msec(int code) {
	if (code < 0 || code >= FTX_1_DELAY_CODES) {
		return -1;
	}
	return FTX_1_DELAY_MSEC[code];
}

/// NB / NR level clamped to 1 - 10
static int dsp_level(int level) {
	if (level < 1) {
		return 1;
	}
	if (level > 10) {
		return 10;
	}
	return level;
}

/// mode index for an MD answer character; unknown characters give LSB
static int mode_from_chr(char mode_chr) {
	for (int mode = 0; mode <= mC4FMVW; mode++) {
		if (FTX_1_MODE_CHR[mode] == mode_chr) {
			return mode;
		}
	}
	return mLSB;
}

//----------------------------------------------------------------------
static std::vector<std::string> ftx1_att_labels;
static const char *FTX_1_ATT_LABELS[] = { "ATT", "12 dB" };

// PA P1 0 (HF/50 MHz): 0 IPO, 1 AMP1, 2 AMP2
static std::vector<std::string> ftx1_pre_labels_hf;
static const char *FTX_1_PRE_LABELS_HF[] = { "IPO", "AMP 1", "AMP 2" };

// PA P1 1 (144 MHz), 2 (430 MHz): 0 off, 1 on
static std::vector<std::string> ftx1_pre_labels_vu;
static const char *FTX_1_PRE_LABELS_VU[] = { "PRE", "PRE on" };
//----------------------------------------------------------------------

static GUI rig_widgets[] = {
	{ (Fl_Widget *)btnVol,        2, 125,  50 }, // 0
	{ (Fl_Widget *)sldrVOLUME,   54, 125, 368 }, // 1
	{ (Fl_Widget *)sldrRFGAIN,   54, 145, 156 }, // 2
	{ (Fl_Widget *)sldrSQUELCH, 266, 145, 156 }, // 3

	{ (Fl_Widget *)sldrMICGAIN,  54, 165, 156 }, // 4
	{ (Fl_Widget *)btnNotch,    214, 165,  50 }, // 5
	{ (Fl_Widget *)sldrNOTCH,   266, 165, 156 }, // 6

	{ (Fl_Widget *)btnNR,         2, 185,  50 }, // 7
	{ (Fl_Widget *)sldrNR,       54, 185, 156 }, // 8
	{ (Fl_Widget *)btnIFsh,     214, 185,  50 }, // 9
	{ (Fl_Widget *)sldrIFSHIFT, 266, 185, 156 }, // 10

	{ (Fl_Widget *)sldrPOWER,    54, 205, 368 }, // 11

	{ (Fl_Widget *)NULL,          0,   0,   0 }
};

RIG_FTX_1::RIG_FTX_1() {
// base class values
	IDstr = "ID";
	name_ = FTX_1_NAME;
	modes_ = ftx1_modes;
	bandwidths_ = ftx1_widths_ssb;
	bw_vals_ = FTX_1_WVALS_SSB;

	widgets = rig_widgets;

	serial_baudrate = BR38400;
	stopbits = 1;
	serial_retries = 2;

	serial_write_delay = 0;
	serial_post_write_delay = 5;

	serial_timeout = 50;
	serial_rtscts = true;
	serial_rtsplus = false;
	serial_dtrplus = false;
	serial_catptt = true;
	serial_rtsptt = false;
	serial_dtrptt = false;

	A.imode = B.imode = modeB = modeA = def_mode = 1;
	A.iBW = B.iBW = bwA = bwB = def_bw = 12;
	A.freq = B.freq = freqA = freqB = def_freq = 14070000ULL;

	has_compression =
	has_compON =
	has_a2b =
	has_ext_tuner =
	has_xcvr_auto_on_off =
	has_split =
	has_noise_reduction =
	has_noise_reduction_control =
	has_extras =
	has_vox_onoff =
	has_vox_gain =
	has_vox_hang =
	has_vox_on_dataport =

	has_vfo_adj =

	has_cw_wpm =
	has_cw_keyer =
	has_cw_vol =
	has_cw_spot =
	has_cw_spot_tone =
	has_cw_qsk =
	has_cw_delay =
	has_cw_weight =
	has_cw_zero_in =

	has_band_selection =

	can_change_alt_vfo =
	has_smeter =
	has_alc_control =
	has_swr_control =
	has_power_out =
	has_power_control =
	has_volume_control =
	has_rf_control =
	has_sql_control =
	has_micgain_control =
	has_mode_control =
	has_nb_level =
	has_noise_control =
	has_bandwidth_control =
	has_notch_control =
	has_auto_notch =
	has_attenuator_control =
	has_preamp_control =
	has_ifshift_control =
	has_ptt_control =
	has_tune_control = true;

// derived specific
	m_atten_level = 1;
	m_head = '0';

	precision = 1;
	ndigits = 9;
}

/// build the tables and set first-time defaults
void RIG_FTX_1::initialize() {
	VECTOR(ftx1_modes, FTX_1_MODE_NAMES);
	VECTOR(ftx1_widths_ssb, FTX_1_WIDTHS_SSB);
	VECTOR(ftx1_widths_data, FTX_1_WIDTHS_DATA);
	VECTOR(ftx1_widths_cw, FTX_1_WIDTHS_CW);
	VECTOR(ftx1_widths_am, FTX_1_WIDTHS_AM);
	VECTOR(ftx1_widths_fmn, FTX_1_WIDTHS_FMN);
	VECTOR(ftx1_widths_fm, FTX_1_WIDTHS_FM);

	VECTOR(ftx1_att_labels, FTX_1_ATT_LABELS);
	att_labels_ = ftx1_att_labels;

	VECTOR(ftx1_pre_labels_hf, FTX_1_PRE_LABELS_HF);
	VECTOR(ftx1_pre_labels_vu, FTX_1_PRE_LABELS_VU);
	pre_labels_ = ftx1_pre_labels_hf;

	modes_ = ftx1_modes;
	bandwidths_ = ftx1_widths_ssb;
	bw_vals_ = FTX_1_WVALS_SSB;

	rig_widgets[0].W = btnVol;
	rig_widgets[1].W = sldrVOLUME;
	rig_widgets[2].W = sldrRFGAIN;
	rig_widgets[3].W = sldrSQUELCH;
	rig_widgets[4].W = sldrMICGAIN;
	rig_widgets[5].W = btnNotch;
	rig_widgets[6].W = sldrNOTCH;
	rig_widgets[7].W = btnNR;
	rig_widgets[8].W = sldrNR;
	rig_widgets[9].W = btnIFsh;
	rig_widgets[10].W = sldrIFSHIFT;
	rig_widgets[11].W = sldrPOWER;

// set progStatus defaults
	if (progStatus.notch_val < 10) {
		progStatus.notch_val = 1500;
	}
	if (progStatus.noise_reduction_val < 1) {
		progStatus.noise_reduction_val = 1;
	}
	if (progStatus.power_level < 5) {
		progStatus.power_level = 5;
	}
	m_nb_on = progStatus.noise;
	m_nb_level = dsp_level(progStatus.nb_level);
	m_nr_on = (progStatus.noise_reduction != 0);
	m_nr_level = dsp_level(progStatus.noise_reduction_val);

// first-time-thru, or reset
	if (progStatus.cw_qsk < 15) {
		progStatus.cw_qsk = 15;
		progStatus.cw_spot_tone = 700;
		progStatus.cw_weight = 3.0;
		progStatus.cw_wpm = 18;
		progStatus.vox_on_dataport = false;
		progStatus.vox_gain = 50;
		progStatus.vox_hang = 500;
	}

// Disable Auto Information mode
//	sendCommand("AI0;");

	op_yaesu_select60->deactivate();
}

/// nothing to do after the UI is set up
void RIG_FTX_1::post_initialize() {
}

/// true if the radio answers ID
bool RIG_FTX_1::check() {
	cmd = "ID;";
	get_trace(1, __func__);
	wait_char(';', 7, 500, __func__, ASC);
	gett("");
	if (replystr.rfind("ID") != std::string::npos) {
		return true;
	}
	return false;
}

/// The FTX-1 has two receivers.  flrig VFO A is the MAIN side and VFO B is
/// the SUB side.  VS selects which side is in use; FA/FB, MD0/MD1 and
/// SH0/SH1 always address MAIN and SUB directly.
/// Returns the CAT P1 digit for the side in use: '0' MAIN, '1' SUB.
char RIG_FTX_1::active_side() {
	if (inuse == onB) {
		return '1';
	}
	return '0';
}

/// FA, MAIN side frequency
unsigned long long RIG_FTX_1::get_vfoA() {
	cmd = rsp = "FA";
	cmd += ';';

	get_trace(1, __func__);
	wait_char(';', 12, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return freqA;
	}
	pos += 2;
	unsigned long long frequency = 0;
	for (int n = 0; n < ndigits; n++) {
		frequency = frequency * 10 + replystr[pos + n] - '0';
	}
	freqA = frequency;
	return freqA;
}

/// FA, MAIN side frequency
void RIG_FTX_1::set_vfoA(unsigned long long frequency) {
	freqA = frequency;
	cmd = "FA000000000;";
	for (int i = 0; i < ndigits; i++) {
		cmd[ndigits + 1 - i] += frequency % 10;
		frequency /= 10;
	}

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// FB, SUB side frequency
unsigned long long RIG_FTX_1::get_vfoB() {
	cmd = rsp = "FB";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 12, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return freqB;
	}
	pos += 2;
	unsigned long long frequency = 0;
	for (int n = 0; n < ndigits; n++) {
		frequency = frequency * 10 + replystr[pos + n] - '0';
	}
	freqB = frequency;
	return freqB;
}

/// FB, SUB side frequency
void RIG_FTX_1::set_vfoB(unsigned long long frequency) {
	freqB = frequency;
	cmd = "FB000000000;";
	for (int i = 0; i < ndigits; i++) {
		cmd[ndigits + 1 - i] += frequency % 10;
		frequency /= 10;
	}

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// Read which side the radio is using, so a MAIN/SUB change made on the
/// front panel is followed by flrig.
int RIG_FTX_1::get_vfoAorB() {
	cmd = rsp = "VS";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 4, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos != std::string::npos && pos + 2 < replystr.length()) {
		if (replystr[pos + 2] == '1') {
			inuse = onB;
		} else {
			inuse = onA;
		}
	}
	return inuse;
}

/// VS0, use the MAIN side
void RIG_FTX_1::selectA() {
	cmd = "VS0;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

	inuse = onA;
}

/// VS1, use the SUB side
void RIG_FTX_1::selectB() {
	cmd = "VS1;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

	inuse = onB;
}

/// AB, copy MAIN to SUB
void RIG_FTX_1::A2B() {
	cmd = "AB;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// BA, copy SUB to MAIN
void RIG_FTX_1::B2A() {
	cmd = "BA;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// SV, swap MAIN and SUB
void RIG_FTX_1::swapAB() {
	cmd = "SV;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// split is available
bool RIG_FTX_1::can_split() {
	return true;
}

/// Split is done by moving the transmitter to the other side with FT.
/// ST1 is not used: on the FTX-1 it locks the SUB side in transmit.
void RIG_FTX_1::set_split(bool split_on) {
	split = split_on;
	bool tx_on_sub = (inuse == onA);
	if (!split_on) {
		tx_on_sub = !tx_on_sub;
	}
	if (tx_on_sub) {
		cmd = "FT1;";
	} else {
		cmd = "FT0;";
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// split is on when FT is not the side in use
int RIG_FTX_1::get_split() {
	cmd = rsp = "FT";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 4, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 2 >= replystr.length()) {
		return split;
	}
	split = (replystr[pos + 2] != active_side());
	return split;
}

/// SM, S meter of the side in use, 0 - 100
int RIG_FTX_1::get_smeter() {
	cmd = "SM0;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind("SM");
	if (pos == std::string::npos) {
		return 0;
	}
	if (pos + 6 >= replystr.length()) {
		return 0;
	}
	int meter = atoi(&replystr[pos + 3]);
	meter = meter / 2.56;
	return meter;
}

/// read RM meter P1 (4 ALC, 5 PO, 6 SWR), 0 - 255
/// answer is RM P1 P2P2P2 P3P3P3; with P3 fixed 000
int RIG_FTX_1::get_meter(char meter) {
	cmd = rsp = "RM0";
	cmd[2] = rsp[2] = meter;
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 10, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 6 >= replystr.length()) {
		return 0;
	}
	return atoi(replystr.substr(pos + 3, 3).c_str());
}

/// RM6, SWR meter 0 - 100
int RIG_FTX_1::get_swr() {
	return (int)ceil(get_meter('6') / 2.56);
}

/// RM4, ALC meter 0 - 100
int RIG_FTX_1::get_alc() {
	return (int)ceil(get_meter('4') / 2.56);
}

// RM5 (PO) to watts.  PROVISIONAL: these are not measured on an FTX-1.
// The SPA-1 table is the FT-891 100 W curve and the field head table is
// the same curve scaled to 10 W.  They need to be replaced with readings
// taken against a wattmeter.
static const meterpair FTX_1_PWRTBL_SPA1[] = {
	{   0,   0.0 },
	{  35,   5.0 },
	{  59,  10.0 },
	{  72,  15.0 },
	{  86,  20.0 },
	{  96,  25.0 },
	{ 107,  30.0 },
	{ 119,  35.0 },
	{ 129,  40.0 },
	{ 141,  45.0 },
	{ 150,  50.0 },
	{ 155,  55.0 },
	{ 161,  60.0 },
	{ 166,  65.0 },
	{ 173,  70.0 },
	{ 179,  75.0 },
	{ 184,  80.0 },
	{ 191,  85.0 },
	{ 196,  90.0 },
	{ 202,  95.0 },
	{ 208, 100.0 }
};

static const meterpair FTX_1_PWRTBL_FIELD[] = {
	{   0,  0.0 },
	{  35,  0.5 },
	{  59,  1.0 },
	{  72,  1.5 },
	{  86,  2.0 },
	{  96,  2.5 },
	{ 107,  3.0 },
	{ 119,  3.5 },
	{ 129,  4.0 },
	{ 141,  4.5 },
	{ 150,  5.0 },
	{ 155,  5.5 },
	{ 161,  6.0 },
	{ 166,  6.5 },
	{ 173,  7.0 },
	{ 179,  7.5 },
	{ 184,  8.0 },
	{ 191,  8.5 },
	{ 196,  9.0 },
	{ 202,  9.5 },
	{ 208, 10.0 }
};

/// RM5, power out in watts using the table for the fitted head
int RIG_FTX_1::get_power_out() {
	int meter = get_meter('5');

	const meterpair *pwrtbl = FTX_1_PWRTBL_SPA1;
	size_t entries = sizeof(FTX_1_PWRTBL_SPA1) / sizeof(*FTX_1_PWRTBL_SPA1);
	if (m_head == '1') {
		pwrtbl = FTX_1_PWRTBL_FIELD;
		entries = sizeof(FTX_1_PWRTBL_FIELD) / sizeof(*FTX_1_PWRTBL_FIELD);
	}

	size_t i = 0;
	for (i = 0; i < entries - 1; i++) {
		if (meter >= pwrtbl[i].mtr && meter < pwrtbl[i + 1].mtr) {
			break;
		}
	}
	if (i == entries - 1) {
		return (int)ceil(pwrtbl[i].val);
	}
	int watts = (int)ceil(
		pwrtbl[i].val +
		(pwrtbl[i + 1].val - pwrtbl[i].val) * (meter - pwrtbl[i].mtr) /
		(pwrtbl[i + 1].mtr - pwrtbl[i].mtr));

	return watts;
}

// PC P1 P2
//   P1 1: FTX-1 field head, 0.5 - 10 W (6 W on battery), may answer
//         with tenths, e.g. PC10.5;
//   P1 2: SPA-1, 5 - 100 W, whole watts PC2005;

/// Find out which head is fitted from the PC answer.  m_head is '1' field
/// head, '2' SPA-1, '0' not known yet.
void RIG_FTX_1::read_head() {
	cmd = rsp = "PC";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 2 >= replystr.length()) {
		return;
	}
	if (replystr[pos + 2] == '1' || replystr[pos + 2] == '2') {
		m_head = replystr[pos + 2];
	}
}

/// power range for the fitted head
void RIG_FTX_1::get_pc_min_max_step(double &min, double &max, double &step) {
	if (m_head == '0') {
		read_head();
	}
	if (m_head == '1') {
		min = 0.5;
		max = 10;
		step = 0.5;
	} else {
		min = 5;
		max = 100;
		step = 1;
	}
	pmax = max;
}

/// PC, power setting in watts
double RIG_FTX_1::get_power_control() {
	read_head();

	size_t pos = replystr.rfind("PC");
	if (pos == std::string::npos || pos + 3 >= replystr.length()) {
		return progStatus.power_level;
	}
	return atof(replystr.substr(pos + 3).c_str());
}

/// PC, power setting in watts for the fitted head
void RIG_FTX_1::set_power_control(double watts) {
	if (m_head == '0') {
		read_head();
	}
	char cmdstr[20];
	if (m_head == '1') {
		if (watts < 0.5) {
			watts = 0.5;
		}
		if (watts > 10) {
			watts = 10;
		}
		int tenths = (int)round(watts * 10);
		if (tenths % 10 == 0) {
			snprintf(cmdstr, sizeof(cmdstr), "PC1%03d;", tenths / 10);
		} else {
			snprintf(cmdstr, sizeof(cmdstr), "PC1%.1f;", tenths / 10.0);
		}
	} else if (m_head == '2') {
		if (watts < 5) {
			watts = 5;
		}
		if (watts > 100) {
			watts = 100;
		}
		snprintf(cmdstr, sizeof(cmdstr), "PC2%03d;", (int)round(watts));
	} else {
		LOG_WARN("FTX-1 head type unknown, power not set");
		return;
	}
	cmd = cmdstr;
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// AG, AF gain of the side in use, 0 - 100
int RIG_FTX_1::get_volume_control() {
	cmd = "AG0;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind("AG");
	if (pos == std::string::npos) {
		return progStatus.volume;
	}
	if (pos + 6 >= replystr.length()) {
		return progStatus.volume;
	}
	int volume = round(atoi(&replystr[pos + 3]) / 2.55);
	if (volume > 100) {
		volume = 100;
	}
	return ceil(volume);
}

/// AG, AF gain of the side in use, 0 - 100 scaled to 000 - 255
void RIG_FTX_1::set_volume_control(int volume) {
	int af_level = (int)(volume * 2.55);
	cmd = "AG0000;";
	cmd[2] = active_side();
	for (int i = 5; i > 2; i--) {
		cmd[i] += af_level % 10;
		af_level /= 10;
	}

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// TX, transmitter on/off
void RIG_FTX_1::set_PTT_control(int ptt_on) {
	if (ptt_on) {
		cmd = "TX1;";
	} else {
		cmd = "TX0;";
	}

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
	ptt_ = ptt_on;
}

/// TX, 1 if transmitting
int RIG_FTX_1::get_PTT() {
	cmd = "TX;";

	get_trace(1, __func__);
	wait_char(';', 4, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind("TX");
	if (pos == std::string::npos) {
		return ptt_;
	}
	if (replystr[pos + 2] != '0') {
		ptt_ = 1;
	} else {
		ptt_ = 0;
	}

	return ptt_;
}

// AC P1 P2 P3
//   P1 0 internal tuner, 1 external tuner (flrig "external tuner")
//   P2 0 antenna tuner
//   P3 0 off / stop, 1 on, 3 start tuning

/// AC, tuner off (0), on (1) or start tuning (2)
void RIG_FTX_1::tune_rig(int action) {
	switch (action) {
		case 0:
			cmd = "AC000;";
			break;
		case 1:
			cmd = "AC001;";
			break;
		case 2:
		default:
			cmd = "AC003;";
			break;
	}
	if (action != 0 && progStatus.external_tuner) {
		cmd[2] = '1';
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// AC, 1 if the tuner is on or tuning
int RIG_FTX_1::get_tune() {
	cmd = rsp = "AC";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 6, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 4 >= replystr.length()) {
		return 0;
	}
	return replystr[pos + 4] != '0';
}

/// RA, attenuator on/off
void RIG_FTX_1::set_attenuator(int atten_on) {
	if (atten_on) {
		cmd = "RA01;";
	} else {
		cmd = "RA00;";
	}

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// RA, attenuator on/off
int RIG_FTX_1::get_attenuator() {
	cmd = "RA0;";
	get_trace(1, __func__);
	wait_char(';', 5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind("RA");
	if (pos == std::string::npos) {
		return progStatus.attenuator;
	}
	if (pos + 3 >= replystr.length()) {
		return progStatus.attenuator;
	}
	m_atten_level = replystr[pos + 3] - '0';

	return m_atten_level;
}

/// PA P1 for the active side's frequency: '0' HF/50, '1' 144, '2' 430
char RIG_FTX_1::preamp_band() {
	unsigned long long frequency = freqA;
	if (inuse == onB) {
		frequency = freqB;
	}
	if (frequency >= 420000000ULL) {
		return '2';
	}
	if (frequency >= 144000000ULL) {
		return '1';
	}
	return '0';
}

/// highest preamp setting: AMP2 on HF/50, on/off on 144 and 430
int RIG_FTX_1::preamp_max(char band) {
	if (band == '0') {
		return 2;
	}
	return 1;
}

/// use the button labels for the band
void RIG_FTX_1::preamp_labels(char band) {
	if (band == '0') {
		pre_labels_ = ftx1_pre_labels_hf;
	} else {
		pre_labels_ = ftx1_pre_labels_vu;
	}
}

/// preamp setting after the current one, for the preamp button
int RIG_FTX_1::next_preamp() {
	if (preamp_state >= preamp_max(preamp_band())) {
		return 0;
	}
	return preamp_state + 1;
}

/// PA, preamp setting for the band
void RIG_FTX_1::set_preamp(int preamp) {
	char band = preamp_band();
	if (preamp < 0) {
		preamp = 0;
	}
	if (preamp > preamp_max(band)) {
		preamp = preamp_max(band);
	}
	preamp_state = preamp;
	preamp_labels(band);

	cmd = "PA00;";
	cmd[2] = band;
	cmd[3] = '0' + preamp;
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// PA, preamp setting for the band
int RIG_FTX_1::get_preamp() {
	char band = preamp_band();
	cmd = rsp = "PA0";
	cmd[2] = rsp[2] = band;
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos != std::string::npos && pos + 3 < replystr.length()) {
		int value = replystr[pos + 3] - '0';
		if (value >= 0 && value <= preamp_max(band)) {
			preamp_state = value;
		}
	}
	preamp_labels(band);
	return preamp_state;
}

/// true for modes where the FTX-1 has one fixed bandwidth and SH does
/// not apply
bool RIG_FTX_1::fixed_width(int mode) {
	switch (mode) {
		case mAM:
		case mAMN:
		case mFM:
		case mFMN:
		case mDATAFM:
		case mDATAFMN:
		case mC4FMDN:
		case mC4FMVW:
			return true;
		default:
			return false;
	}
}

/// select the width tables for the mode, returns the default width index
int RIG_FTX_1::adjust_bandwidth(int mode) {
	bandwidths_ = bwtable(mode);
	switch (mode) {
		case mCW_U:
		case mCW_L:
		case mRTTYL:
		case mRTTYU:
			bw_vals_ = FTX_1_WVALS_CW;
			break;
		case mDATAL:
		case mDATAU:
		case mPSK:
			bw_vals_ = FTX_1_WVALS_DATA;
			break;
		case mAM:
		case mAMN:
			bw_vals_ = FTX_1_WVALS_AM;
			break;
		case mFMN:
		case mDATAFMN:
			bw_vals_ = FTX_1_WVALS_FMN;
			break;
		case mFM:
		case mDATAFM:
		case mC4FMDN:
		case mC4FMVW:
			bw_vals_ = FTX_1_WVALS_FM;
			break;
		default:
			bw_vals_ = FTX_1_WVALS_SSB;
	}
	return FTX_1_DEF_BW[mode];
}

/// default width index for the mode
int RIG_FTX_1::def_bandwidth(int mode) {
	return FTX_1_DEF_BW[mode];
}

/// width names for the mode
std::vector<std::string>& RIG_FTX_1::bwtable(int mode) {
	switch (mode) {
		case mCW_U:
		case mCW_L:
		case mRTTYL:
		case mRTTYU:
			return ftx1_widths_cw;
		case mDATAL:
		case mDATAU:
		case mPSK:
			return ftx1_widths_data;
		case mAM:
		case mAMN:
			return ftx1_widths_am;
		case mFMN:
		case mDATAFMN:
			return ftx1_widths_fmn;
		case mFM:
		case mDATAFM:
		case mC4FMDN:
		case mC4FMVW:
			return ftx1_widths_fm;
		default:
			return ftx1_widths_ssb;
	}
}

/// Set the width for one side.  NARROW is turned off first because with
/// NARROW on the FTX-1 uses the menu NAR WIDTH and the SH setting does not
/// hold.
void RIG_FTX_1::set_width(char side, int bw_code) {
	cmd = "NA00;";
	cmd[2] = side;
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

	cmd = "SH00";
	cmd[2] = side;
	cmd += '0' + bw_code / 10;
	cmd += '0' + bw_code % 10;
	cmd += ';';
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// Width index for the SH answer starting with answer ("SH00" or "SH10").
/// Returns -1 if the answer was not received.  A code not in the table
/// gives the last index.
int RIG_FTX_1::read_width(const char *answer) {
// answer is SH P1 0 nn;   nn = width code
	size_t pos = replystr.rfind(answer);
	if (pos == std::string::npos) {
		return -1;
	}
	if (pos + 6 >= replystr.length()) {
		return -1;
	}

	int bw_code = 0;
	sscanf(&replystr[pos + 4], "%d;", &bw_code);

	const int *wval = bw_vals_;
	int index = 0;
	while (*wval != WVALS_LIMIT) {
		if (*wval == bw_code) {
			break;
		}
		wval++;
		index++;
	}
	if (*wval == WVALS_LIMIT) {
		index--;
	}
	return index;
}

/// MD0, MAIN side mode
void RIG_FTX_1::set_modeA(int mode) {
	modeA = mode;

	adjust_bandwidth(modeA);

	cmd = "MD0";
	cmd += FTX_1_MODE_CHR[mode];
	cmd += ';';

	set_trace(1, "set_modeA()");
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, "SET mode A", cmd, replystr);
}

/// MD0, MAIN side mode
int RIG_FTX_1::get_modeA() {
	cmd = "MD0;";
	get_trace(1, __func__);
	wait_char(';', 5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind("MD");
	if (pos != std::string::npos && pos + 3 < replystr.length()) {
		modeA = mode_from_chr(replystr[pos + 3]);
	}

	adjust_bandwidth(modeA);

	return modeA;
}

/// MD1, SUB side mode
void RIG_FTX_1::set_modeB(int mode) {
	modeB = mode;

	adjust_bandwidth(modeB);

	cmd = "MD1";
	cmd += FTX_1_MODE_CHR[mode];
	cmd += ';';

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// MD1, SUB side mode
int RIG_FTX_1::get_modeB() {
	cmd = "MD1;";
	get_trace(1, __func__);
	wait_char(';', 5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind("MD");
	if (pos != std::string::npos && pos + 3 < replystr.length()) {
		modeB = mode_from_chr(replystr[pos + 3]);
	}

	adjust_bandwidth(modeB);
	return modeB;
}

/// SH0, MAIN side width
void RIG_FTX_1::set_bwA(int bw_index) {
	bwA = bw_index;
	if (fixed_width(modeA)) {
		return;
	}
	set_width('0', bw_vals_[bw_index]);
}

/// SH0, MAIN side width
int RIG_FTX_1::get_bwA() {
	if (fixed_width(modeA)) {
		bwA = 0;
		return bwA;
	}

	cmd = "SH0;";

	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	int index = read_width("SH00");
	if (index >= 0) {
		bwA = index;
	}
	return bwA;
}

/// SH1, SUB side width
void RIG_FTX_1::set_bwB(int bw_index) {
	bwB = bw_index;
	if (fixed_width(modeB)) {
		return;
	}
	set_width('1', bw_vals_[bw_index]);
}

/// SH1, SUB side width
int RIG_FTX_1::get_bwB() {
	if (fixed_width(modeB)) {
		bwB = 0;
		return bwB;
	}
	cmd = "SH1;";
	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	int index = read_width("SH10");
	if (index >= 0) {
		bwB = index;
	}
	return bwB;
}

/// 'L' or 'U' sideband for the mode
int RIG_FTX_1::get_modetype(int mode) {
	return FTX_1_MODE_TYPE[mode];
}

/// IS, IF shift of the side in use, -1200 - +1200 Hz
void RIG_FTX_1::set_if_shift(int shift) {
	char cmdstr[20];
	snprintf(cmdstr, sizeof(cmdstr), "IS%c0%+05d;", active_side(), shift);
	cmd = cmdstr;

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// IS, IF shift of the side in use
bool RIG_FTX_1::get_if_shift(int &shift) {
	if (inuse == onA) {
		cmd = "IS0;";
	} else {
		cmd = "IS1;";
	}
	get_trace(1, __func__);
	wait_char(';', 10, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind("IS");
	if (pos == std::string::npos) {
		shift = progStatus.shift_val;
		return true;
	}

	char side = '0';
	int value = progStatus.shift_val;
	sscanf(&replystr[pos], "IS%c0%d;", &side, &value);

	shift = value;
	return true;
}

/// BP, manual notch on/off and frequency for the side in use
void RIG_FTX_1::set_notch(bool notch_on, int frequency) {
// set notch frequency
	if (notch_on) {
		cmd = "BP00001;";
		cmd[2] = active_side();
		set_trace(1, __func__);
		sendCommand(cmd);
		sett("");
		showresp(WARN, ASC, __func__, cmd, replystr);
		cmd = "BP01000;";
		cmd[2] = active_side();
		if (frequency % 10 >= 5) {
			frequency += 10;
		}
		frequency /= 10;
		for (int i = 3; i > 0; i--) {
			cmd[3 + i] += frequency % 10;
			frequency /= 10;
		}
		set_trace(1, "set notch value");
		sendCommand(cmd);
		sett("");
		showresp(WARN, ASC, "SET notch val", cmd, replystr);
		return;
	}

// set notch off
	cmd = "BP00000;";
	cmd[2] = active_side();
	set_trace(1, "set_notch OFF");
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, "SET notch off", cmd, replystr);
}

/// BP, true if the manual notch is on; frequency is set when it is
bool RIG_FTX_1::get_notch(int &frequency) {
	bool is_on = false;
	cmd = "BP00;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';', 8, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind("BP");
	if (pos == std::string::npos || pos + 6 >= replystr.length()) {
		return is_on;
	}

// manual notch enabled
	if (replystr[pos + 6] == '1') {
		is_on = true;
		frequency = progStatus.notch_val;
		cmd = "BP01;";
		cmd[2] = active_side();
		get_trace(1, "get notch value()");
		wait_char(';', 8, FTX_1_WAIT_TIME, "get notch val", ASC);
		gett("");
		pos = replystr.rfind("BP");
		if (pos != std::string::npos && pos + 6 < replystr.length()) {
			frequency = fm_decimal(replystr.substr(pos + 4), 3) * 10;
		}
	}
	return is_on;
}

/// BC, auto notch (DNF) on/off for the side in use
void RIG_FTX_1::set_auto_notch(int notch_on) {
	cmd.assign("BC").append(1, active_side());
	cmd.append(notch_on ? "1" : "0").append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// BC, auto notch (DNF) on/off for the side in use
int RIG_FTX_1::get_auto_notch() {
	cmd = "BC0;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';', 5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");
	size_t pos = replystr.rfind("BC");
	if (pos == std::string::npos) {
		return 0;
	}
	if (replystr[pos + 3] == '1') {
		return 1;
	}
	return 0;
}

// The FTX-1 has no NB or NR on/off command; level 0 is off.
//   NL P1 P2P2P2  noise blanker level 000 off, 001 - 010
//   RL P1 P2P2    noise reduction (DNR) level 00 off, 01 - 10
// The driver keeps the on/off state and level it last sent or read.
// While a function is off its level is only saved, so that setting the
// level does not turn it back on.

/// send NL (noise blanker) or RL (noise reduction) for the active side
void RIG_FTX_1::set_dsp_level(const char *command, int digits, int level) {
	cmd.assign(command).append(1, active_side());
	cmd.append(to_decimal(level, digits)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// read NL or RL for the active side, -1 if no valid reply
int RIG_FTX_1::get_dsp_level(const char *command, int digits) {
	cmd.assign(command).append(1, active_side()).append(";");
	get_trace(1, __func__);
	wait_char(';', 4 + digits, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(command);
	if (pos == std::string::npos || pos + 3 + digits >= replystr.length()) {
		return -1;
	}
	return atoi(replystr.substr(pos + 3, digits).c_str());
}

/// read NL or RL and update the saved on/off state and level
void RIG_FTX_1::read_dsp(const char *command, int digits,
	bool &is_on, int &level) {
	int value = get_dsp_level(command, digits);
	if (value < 0) {
		return;
	}
	is_on = (value > 0);
	if (is_on) {
		level = dsp_level(value);
	}
}

/// NL, noise blanker on/off
void RIG_FTX_1::set_noise(bool nb_on) {
	m_nb_on = nb_on;
	set_dsp_level("NL", 3, nb_on ? m_nb_level : 0);
}

/// NL, noise blanker on/off
int RIG_FTX_1::get_noise() {
	read_dsp("NL", 3, m_nb_on, m_nb_level);
	return m_nb_on;
}

/// NL, noise blanker level, sent only while the noise blanker is on
void RIG_FTX_1::set_nb_level(int level) {
	m_nb_level = dsp_level(level);
	if (m_nb_on) {
		set_dsp_level("NL", 3, m_nb_level);
	}
}

/// NL, noise blanker level
int RIG_FTX_1::get_nb_level() {
	read_dsp("NL", 3, m_nb_on, m_nb_level);
	return m_nb_level;
}

/// RL, noise reduction (DNR) on/off
void RIG_FTX_1::set_noise_reduction(int nr_on) {
	m_nr_on = (nr_on != 0);
	set_dsp_level("RL", 2, m_nr_on ? m_nr_level : 0);
}

/// RL, noise reduction (DNR) on/off
int RIG_FTX_1::get_noise_reduction() {
	read_dsp("RL", 2, m_nr_on, m_nr_level);
	return m_nr_on;
}

/// RL, noise reduction level, sent only while noise reduction is on
void RIG_FTX_1::set_noise_reduction_val(int level) {
	m_nr_level = dsp_level(level);
	if (m_nr_on) {
		set_dsp_level("RL", 2, m_nr_level);
	}
}

/// RL, noise reduction level
int RIG_FTX_1::get_noise_reduction_val() {
	read_dsp("RL", 2, m_nr_on, m_nr_level);
	return m_nr_level;
}

/// MG, mic gain 0 - 100
void RIG_FTX_1::set_mic_gain(int gain) {
	cmd = "MG000;";
	for (int i = 4; i > 1; i--) {
		cmd[i] = gain % 10 + '0';
		gain /= 10;
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// MG, mic gain 0 - 100
int RIG_FTX_1::get_mic_gain() {
	cmd = "MG;";
	get_trace(1, __func__);
	wait_char(';', 6, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind("MG");
	if (pos == std::string::npos) {
		return progStatus.mic_gain;
	}
	int gain = atoi(&replystr[pos + 2]);
	if (gain > 100) {
		gain = 100;
	}
	return ceil(gain);
}

/// RG, RF gain of the side in use, 0 - 100 scaled to 000 - 255
void RIG_FTX_1::set_rf_gain(int gain) {
	int rf_level = round(gain * 2.55);
	cmd.assign("RG").append(1, active_side());
	cmd.append(to_decimal(rf_level, 3)).append(";");

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// RG, RF gain of the side in use, 0 - 100
int RIG_FTX_1::get_rf_gain() {
	cmd = rsp = "RG0";
	cmd[2] = rsp[2] = active_side();
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 6 >= replystr.length()) {
		return progStatus.rfgain;
	}
	int rf_level = atoi(replystr.substr(pos + 3, 3).c_str());
	return round(rf_level / 2.55);
}

/// SQ, squelch of the side in use, 0 - 100 scaled to 000 - 255
void RIG_FTX_1::set_squelch(int level) {
	char cmdstr[12];
	snprintf(cmdstr, sizeof(cmdstr), "%c%03d",
		(inuse == onA ? '0' : '1'), level * 255 / 100);
	cmd.assign("SQ").append(cmdstr).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// SQ, squelch of the side in use, 0 - 100
int RIG_FTX_1::get_squelch() {
	if (inuse == onA) {
		cmd = "SQ0;";
	} else {
		cmd = "SQ1;";
	}
	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind("SQ");
	if (pos == std::string::npos) {
		return progStatus.squelch;
	}
	char side = '0';
	int level = 0;
	sscanf(&replystr[pos], "SQ%c%d;", &side, &level);

	return level * 100 / 255;
}

// VX     VOX on/off
// VG     VOX GAIN 000 - 100
// VD     VOX DELAY, 2 digit code (see FTX_1_DELAY_MSEC)
// EX030510 VOX SELECT 0: MIC, 1: USB, 2: Bluetooth
// VG and VD apply to the input chosen by VOX SELECT.
// The FTX-1 has no anti-VOX setting available over CAT.

/// VX, VOX on/off
void RIG_FTX_1::set_vox_onoff() {
	cmd = "VX0;";
	if (progStatus.vox_onoff) {
		cmd[2] = '1';
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// VG, VOX gain
void RIG_FTX_1::set_vox_gain() {
	cmd.assign("VG").append(to_decimal(progStatus.vox_gain, 3)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// VD, VOX delay
void RIG_FTX_1::set_vox_hang() {
	cmd.assign("VD").append(to_decimal(delay_code(progStatus.vox_hang), 2));
	cmd.append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// EX030510 VOX SELECT, MIC or USB (data)
void RIG_FTX_1::set_vox_on_dataport() {
	cmd = "EX0305100;";
	if (progStatus.vox_on_dataport) {
		cmd[8] = '1';
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// KS, keyer speed 4 - 60 WPM
void RIG_FTX_1::set_cw_wpm() {
	cmd = "KS";
	if (progStatus.cw_wpm > 60) {
		progStatus.cw_wpm = 60;
	}
	if (progStatus.cw_wpm < 4) {
		progStatus.cw_wpm = 4;
	}
	cmd.append(to_decimal(progStatus.cw_wpm, 3)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

// ML0 monitor on/off: 000 off, 001 on
// ML1 monitor level 000 - 100

/// ML, CW monitor level; 0 turns the monitor off
void RIG_FTX_1::set_cw_vol() {
	if (progStatus.cw_vol == 0) {
		cmd = "ML0000;";
	} else {
		cmd = "ML0001;";
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

	if (progStatus.cw_vol == 0) {
		return;
	}
	cmd.assign("ML1").append(to_decimal(progStatus.cw_vol, 3)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// ML, CW monitor level, 0 if the monitor is off
int RIG_FTX_1::get_cw_vol() {
	cmd = rsp = "ML0";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");
	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 6 >= replystr.length()) {
		return progStatus.cw_vol;
	}
	if (replystr.substr(pos + 3, 3) == "000") {
		return 0;
	}

	cmd = rsp = "ML1";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");
	pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 6 >= replystr.length()) {
		return progStatus.cw_vol;
	}
	progStatus.cw_vol = atoi(replystr.substr(pos + 3, 3).c_str());
	return progStatus.cw_vol;
}

/// KR, keyer on/off
void RIG_FTX_1::enable_keyer() {
	if (progStatus.enable_keyer) {
		cmd = "KR1;";
	} else {
		cmd = "KR0;";
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// KR, keyer on/off
int RIG_FTX_1::get_keyer() {
	cmd = "KR;";
	get_trace(1, __func__);
	wait_char(';', 4, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");
	size_t pos = replystr.rfind("KR");
	if (pos != std::string::npos) {
		return replystr[pos + 2] - '0';
	}
	return 0;
}

/// CS, CW spot on/off; only in CW
bool RIG_FTX_1::set_cw_spot() {
	if (vfo->imode == mCW_U || vfo->imode == mCW_L) {
		cmd = "CS0;";
		if (progStatus.spot_onoff) {
			cmd[2] = '1';
		}
		set_trace(1, __func__);
		sendCommand(cmd);
		sett("");
		showresp(WARN, ASC, __func__, cmd, replystr);
		return true;
	}
	return false;
}

/// EX020203 CW WEIGHT 25 - 45 (2.5 - 4.5)
void RIG_FTX_1::set_cw_weight() {
	int weight = round(progStatus.cw_weight * 10);
	cmd.assign("EX020203").append(to_decimal(weight, 2)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// EX020117 QSK DELAY TIME 0: 15, 1: 20, 2: 25, 3: 30 msec
void RIG_FTX_1::set_cw_qsk() {
	int qsk_code = progStatus.cw_qsk / 5 - 3;
	if (qsk_code < 0) {
		qsk_code = 0;
	}
	if (qsk_code > 3) {
		qsk_code = 3;
	}
	cmd.assign("EX020117").append(to_decimal(qsk_code, 1)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// EX020117 QSK DELAY TIME in msec
int RIG_FTX_1::get_cw_qsk() {
	cmd = rsp = "EX020117";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 10, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 8 >= replystr.length()) {
		return progStatus.cw_qsk;
	}
	int qsk_code = replystr[pos + 8] - '0';
	if (qsk_code >= 0 && qsk_code <= 3) {
		progStatus.cw_qsk = (qsk_code + 3) * 5;
	}
	return progStatus.cw_qsk;
}

/// SD, CW break-in delay
void RIG_FTX_1::set_cw_delay() {
	cmd.assign("SD").append(to_decimal(delay_code(progStatus.cw_delay), 2));
	cmd.append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// SD, CW break-in delay in msec
int RIG_FTX_1::get_cw_delay() {
	cmd = rsp = "SD";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 4 >= replystr.length()) {
		return progStatus.cw_delay;
	}
	int msec = delay_msec(atoi(replystr.substr(pos + 2, 2).c_str()));
	if (msec > 0) {
		progStatus.cw_delay = msec;
	}
	return progStatus.cw_delay;
}

/// KP, key pitch 00: 300 Hz to 75: 1050 Hz (10 Hz steps)
void RIG_FTX_1::set_cw_spot_tone() {
	int pitch_code = progStatus.cw_spot_tone / 10 - 30;
	if (pitch_code < 0) {
		pitch_code = 0;
	}
	if (pitch_code > 75) {
		pitch_code = 75;
	}
	cmd.assign("KP").append(to_decimal(pitch_code, 2)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

// auto on/off not working well needs much work

/// PS1, turn the radio on
void RIG_FTX_1::set_xcvr_auto_on() {
	if (!progStatus.xcvr_auto_on) {
		return;
	}

// This command requires dummy data be initially sent. Then after one
// second and before two seconds the command is sent.

// use as the dummy data
	cmd = "PS1;";
	sendCommand(cmd);
	update_progress(0);
	for (int i = 0; i < 1200; i += 100) {
		MilliSleep(100);
		update_progress(100 * i / 6000);
		Fl::awake();
	}

	cmd = "PS;";
	get_trace(1, "xcvr ON? ()");
	wait_char(';', 4, 500, "Test: Is Rig ON", ASC);
	gett("");

	if (replystr.find("PS1;") != std::string::npos) {
		update_progress(0);
		return;
	}

	cmd = "PS1;";
	set_trace(1, "set xcvr ON()");
	sendCommand(cmd);
	sett("");

	for (int i = 1500; i < 6000; i += 100) {
		MilliSleep(100);
		update_progress(100 * i / 6000);
		Fl::awake();
	}
}

/// PS0, turn the radio off
void RIG_FTX_1::set_xcvr_auto_off() {
	if (!progStatus.xcvr_auto_off) {
		return;
	}

	cmd = "PS0;";
	set_trace(1, "set_xcvr_OFF()");
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, "SET xcvr auto on/off", cmd, replystr);
}

/// PL speech processor level, PR0 speech processor on/off
void RIG_FTX_1::set_compression(int comp_on, int level) {
	cmd = "PL";
	cmd.append(to_decimal(level, 3)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

// PR0 speech processor, 1 off, 2 on.  Only sent in SSB, where the
// processor applies.
	int current_mode = rigbase::isOnA() ? modeA : modeB;
	if (current_mode == mLSB || current_mode == mUSB) {
		if (comp_on) {
			cmd = "PR02;";
		} else {
			cmd = "PR01;";
		}
		set_trace(2, "set Comp", cmd.c_str());
		set_trace(1, "set_comp_level");
		sendCommand(cmd);
		sett("");
		showresp(WARN, ASC, "set Comp", cmd, replystr);
	}
}

/// PL speech processor level, PR0 speech processor on/off
void RIG_FTX_1::get_compression(int &comp_on, int &level) {
	comp_on = 0;
	level = 0;

	cmd = "PL;";
	get_trace(1, __func__);
	wait_char(';', 6, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind("PL");
	if (pos == std::string::npos) {
		return;
	}
	level = atoi(&replystr[pos + 2]);
	if (level > 100) {
		level = 100;
	}
	level = ceil(level);

// PR0 speech processor, 1 off, 2 on.  Only read in SSB.
	int current_mode = rigbase::isOnA() ? modeA : modeB;
	if (current_mode == mLSB || current_mode == mUSB) {
		cmd = "PR0;";
		get_trace(1, "get comp level");
		wait_char(';', 7, FTX_1_WAIT_TIME, "get PR level", ASC);
		gett("");
		size_t pr_pos = replystr.rfind("PR0");
		if (pr_pos == std::string::npos) {
			return;
		}

		comp_on = (replystr[pr_pos + 3] == '2');
	}

	std::stringstream trace_text;
	trace_text << "get_compression: " << (comp_on ? "ON" : "OFF")
		<< "(" << comp_on << "), comp PL=" << level;
	get_trace(1, trace_text.str().c_str());
}

// Band buttons 1 - 13 (1.8 ... 50, 144, 430, Gen) to BS band codes
//   00 1.8  01 3.5  02 5  03 7  04 10  05 14  06 18  07 21  08 24.5
//   09 28  10 50  11 70/GEN  13 144  14 430

/// BS, band select for the side in use
void RIG_FTX_1::get_band_selection(int band) {
	static const char *BAND_CODES[] = {
		"00", "01", "03", "04", "05", "06", "07", "08", "09", "10",
		"13", "14", "11" };
	if (band < 1 || band > 13) {
		return;
	}
	cmd.assign("BS").append(1, active_side());
	cmd.append(BAND_CODES[band - 1]).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// EX030113 REF FREQ ADJ -25 - +25, always signed: +05, -12
void RIG_FTX_1::setVfoAdj(double adjust) {
	char cmdstr[20];
	snprintf(cmdstr, sizeof(cmdstr), "EX030113%+03d;", (int)adjust);
	cmd = cmdstr;
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// EX030113 REF FREQ ADJ
double RIG_FTX_1::getVfoAdj() {
	cmd = rsp = "EX030113";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 12, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 11 >= replystr.length()) {
		return progStatus.vfo_adj;
	}
	return (double)(atoi(&replystr[pos + 8]));
}

/// REF FREQ ADJ range
void RIG_FTX_1::get_vfoadj_min_max_step(
	double &min,
	double &max,
	double &step
) {
	min = -25;
	max = 25;
	step = 1;
}

//----------------------------------------------------------------------
// AGC control
//----------------------------------------------------------------------

/// GT, AGC of the side in use: 0 off, 1 fast, 2 mid, 3 slow, 4 auto
int RIG_FTX_1::get_agc() {
	cmd = "GT0;";
	cmd[2] = active_side();
	wait_char(';', 6, FTX_1_WAIT_TIME, __func__, ASC);
	gett(__func__);

	size_t pos = replystr.rfind("GT");
	if (pos == std::string::npos) {
		return agcval;
	}

	if (pos + 3 >= replystr.length()) {
		return agcval;
	}
	switch (replystr[pos + 3]) {
		default:
		case '0':
			agcval = 0;
			break;
		case '1':
			agcval = 1;
			break;
		case '2':
			agcval = 2;
			break;
		case '3':
			agcval = 3;
			break;
		case '4':
		case '5':
		case '6':
			agcval = 4;
			break;
	}
	return agcval;
}

/// GT, step to the next AGC setting
int RIG_FTX_1::incr_agc() {
	static const char AGC_CHR[] = { '0', '1', '2', '3', '4' };
	agcval++;
	if (agcval > 4) {
		agcval = 0;
	}
	cmd = "GT00;";
	cmd[2] = active_side();
	cmd[3] = AGC_CHR[agcval];

	sendCommand(cmd);
	showresp(WARN, ASC, __func__, cmd, replystr);
	sett(__func__);

	return agcval;
}

static const char *FTX_1_AGC_LABELS[] = { "AGC", "FST", "MED", "SLO", "AUT" };

/// AGC button label
const char *RIG_FTX_1::agc_label() {
	if (agcval < 0 || agcval > 4) {
		return "AGC";
	}
	return FTX_1_AGC_LABELS[agcval];
}

/// AGC setting 0 - 4
int RIG_FTX_1::agc_val() {
	return agcval;
}

/// ZI, CW auto zero in for the side in use
void RIG_FTX_1::zero_in() {
	cmd = "ZI0;";
	cmd[2] = active_side();
	sendCommand(cmd);
}

/// width name for a width index and mode
const char *RIG_FTX_1::get_bwname_(int bw_index, int mode) {
	try {
		return (bwtable(mode).at(bw_index)).c_str();
	} catch (const std::exception& error) {
		LOG_ERROR("%s", error.what());
	}
	return "";
}
