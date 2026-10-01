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

#include <sstream>
#include "yaesu/FTX_1.h"
#include "debug.h"
#include "support.h"
#include "trace.h"

#define FTX_1_WAIT_TIME 200

enum mFTX_1 { // mode index
   mLSB, mUSB, mCW_U, mFM,  mAM, mRTTYL, mCW_L, mDATAL, mRTTYU, mDATAFM, mFMN, mDATAU, mAMN, mPSK, mDATAFMN, mC4FMDN, mC4FMVW };
//   0    1,      2,   3,    4,    5,     6,      7,       8,      9,     10,    11,    12,   13,     14,       15,      16

static const char FTX_1name_[] = "FTX-1";

static std::vector<std::string>FTX_1modes_;
static const char *vFTX_1modes_[] = {
"LSB",     "USB",     "CW-U",    "FM",      "AM", 
"RTTY-L",  "CW-L",    "DATA-L",  "RTTY-U",  "DATA-FM", 
"FM-N",    "DATA-U",  "AM-N",    "PSK",     "DATA-FM-N", 
"C4FM-DN", "C4FM-VW" };

static const char FTX_1_mode_chr[] =  {
 '1', '2', '3', '4', '5', 
 '6', '7', '8', '9', 'A',
 'B', 'C', 'D', 'E', 'F',
 'H', 'I' };

static const char FTX_1_mode_type[] = {
 'L', 'U', 'U', 'U', 'U', 
 'L', 'L', 'L', 'U', 'U', 
 'U', 'U', 'U', 'U', 'U',
 'U', 'U' };

static const int FTX_1_def_bw[] = {
       17,   17,   5,   0,    0,   10,     5,    16,     10,       0,    0,     16,    0,   16,        0,       0,       0 };
//   mLSB, mUSB, mCW, mFM,  mAM, mRTTYL, mCW_L, mDATAL, mRTTYU, mDATAFM, mFMN, mDATAU, mAMN, mPSK, mDATAFMN, mC4FMDN, mC4FMVW

static std::vector<std::string>FTX_1_widths_SSB;
static const char *vFTX_1_widths_SSB[] = {
 "300",  "400",  "600",  "850", "1100",
"1200", "1500", "1650", "1800", "1950",
"2100", "2250", "2400", "2450", "2500", 
"2600", "2700", "2800", "2900", "3000",
"3200", "3500", "4000" };
static int FTX_1_wvals_SSB[] = {
 1,  2,  3,  4,  5,  
 6,  7,  8,  9, 10,
11, 12, 13, 14, 15, 
16, 17, 18, 19, 20,
21, 22, 23, WVALS_LIMIT};

static std::vector<std::string>FTX_1_widths_DATA;
static const char *vFTX_1_widths_DATA[] = {
   "50",  "100",  "150",  "200",  "250",  
  "300",  "350",  "400",  "450",  "500",
  "600",  "800", "1200", "1400", "1700", 
 "2000", "2400", "3000", "3200", "3500",
 "4000" };
static int FTX_1_wvals_DATA[] = {
 1,  2,  3,  4,  5,  
 6,  7,  8,  9, 10,
11, 12, 13, 14, 15, 
16, 17, 18, 19, 20,
21, WVALS_LIMIT};

static std::vector<std::string>FTX_1_widths_CW;
static const char *vFTX_1_widths_CW[] = {
   "50",  "100",  "150",  "200",  "250",  
  "300",  "350",  "400",  "450",  "500",
  "600",  "800", "1200", "1400", "1700", 
 "2000", "2400", "3000", "3200", "3500",
 "4000" };
static int FTX_1_wvals_CW[] = {
 1,  2,  3,  4,  5,
 6,  7,  8,  9, 10,
11, 12, 13, 14, 15, 
16, 17, 18, 19, 20, 
21, WVALS_LIMIT};

// Single bandwidth modes, CAT manual Table 5
// AM, AM-N
static std::vector<std::string>FTX_1_widths_AM;
static const char *vFTX_1_widths_AM[]  = { "6000" };
static const int FTX_1_wvals_AM[] = { 0, WVALS_LIMIT };

// FM-N, DATA-FM-N
static std::vector<std::string>FTX_1_widths_FMN;
static const char *vFTX_1_widths_FMN[]  = { "9000" };
static const int FTX_1_wvals_FMN[] = { 0, WVALS_LIMIT };

// FM, DATA-FM, C4FM
static std::vector<std::string>FTX_1_widths_FM;
static const char *vFTX_1_widths_FM[]  = { "16000" };
static const int FTX_1_wvals_FM[] = { 0, WVALS_LIMIT };

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

//----------------------------------------------------------------------
static std::vector<std::string>FTX_1_att_labels;
static const char *vFTX_1_att_labels[] = { "ATT", "12 dB" };

// PA P1 0 (HF/50 MHz): 0 IPO, 1 AMP1, 2 AMP2
static std::vector<std::string>FTX_1_pre_labels;
static const char *vFTX_1_pre_labels[] = { "IPO", "AMP 1", "AMP 2" };

// PA P1 1 (144 MHz), 2 (430 MHz): 0 off, 1 on
static std::vector<std::string>FTX_1_pre_labels_vu;
static const char *vFTX_1_pre_labels_vu[] = { "PRE", "PRE on" };
//----------------------------------------------------------------------

static GUI rig_widgets[]= {
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
	name_ = FTX_1name_;
	modes_ = FTX_1modes_;
	bandwidths_ = FTX_1_widths_SSB;
	bw_vals_ = FTX_1_wvals_SSB;

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
//	has_split_AB =
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
	atten_level = 1;
	m_head = '0';
	notch_on = false;
	m_60m_indx = 0;

	precision = 1;
	ndigits = 9;

}

void RIG_FTX_1::initialize()
{
	VECTOR (FTX_1modes_, vFTX_1modes_);
	VECTOR (FTX_1_widths_SSB, vFTX_1_widths_SSB);
	VECTOR (FTX_1_widths_DATA, vFTX_1_widths_DATA);
	VECTOR (FTX_1_widths_CW, vFTX_1_widths_CW);
	VECTOR (FTX_1_widths_AM, vFTX_1_widths_AM);
	VECTOR (FTX_1_widths_FMN, vFTX_1_widths_FMN);
	VECTOR (FTX_1_widths_FM, vFTX_1_widths_FM);

	VECTOR (FTX_1_att_labels, vFTX_1_att_labels);
	att_labels_ = FTX_1_att_labels;

	VECTOR (FTX_1_pre_labels, vFTX_1_pre_labels);
	VECTOR (FTX_1_pre_labels_vu, vFTX_1_pre_labels_vu);
	pre_labels_ = FTX_1_pre_labels;

	modes_ = FTX_1modes_;
	bandwidths_ = FTX_1_widths_SSB;
	bw_vals_ = FTX_1_wvals_SSB;

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
	if (progStatus.notch_val < 10) progStatus.notch_val = 1500;
	if (progStatus.noise_reduction_val < 1) progStatus.noise_reduction_val = 1;
	if (progStatus.power_level < 5) progStatus.power_level = 5;
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

void RIG_FTX_1::post_initialize()
{
}

bool RIG_FTX_1::check ()
{
	cmd = "ID;";
	get_trace(1, __func__);
	wait_char(';', 7, 500, __func__, ASC);
	gett("");
	if (replystr.rfind("ID") != std::string::npos) return true;
	return false;
}

/// The FTX-1 has two receivers.  flrig VFO A is the MAIN side and VFO B is
/// the SUB side.  VS selects which side is in use; FA/FB, MD0/MD1 and
/// SH0/SH1 always address MAIN and SUB directly.
char RIG_FTX_1::active_side() {
	if (inuse == onB) {
		return '1';
	}
	return '0';
}

unsigned long long RIG_FTX_1::get_vfoA ()
{
	cmd = rsp = "FA";
	cmd += ';';

	get_trace(1, __func__);
	wait_char(';',12, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind(rsp);
	if (p == std::string::npos) return freqA;
	p += 2;
	unsigned long long f = 0;
	for (int n = 0; n < ndigits; n++)
		f = f * 10 + replystr[p + n] - '0';
	freqA = f;
	return freqA;
}

void RIG_FTX_1::set_vfoA (unsigned long long freq)
{
	freqA = freq;
	cmd = "FA000000000;";
	for (int i = 0; i < ndigits; i++) {
		cmd[ndigits + 1 - i] += freq % 10;
		freq /= 10;
	}

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

unsigned long long RIG_FTX_1::get_vfoB ()
{
	cmd = rsp = "FB";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';',12, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind(rsp);
	if (p == std::string::npos) return freqB;
	p += 2;
	unsigned long long f = 0;
	for (int n = 0; n < ndigits; n++)
		f = f * 10 + replystr[p + n] - '0';
	freqB = f;
	return freqB;
}


void RIG_FTX_1::set_vfoB (unsigned long long freq)
{
	freqB = freq;
	cmd = "FB000000000;";
	for (int i = 0; i < ndigits; i++) {
		cmd[ndigits + 1 - i] += freq % 10;
		freq /= 10;
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

void RIG_FTX_1::selectA()
{
	cmd = "VS0;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

	inuse = onA;
}

void RIG_FTX_1::selectB()
{
	cmd = "VS1;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

	inuse = onB;
}


void RIG_FTX_1::A2B()
{
	cmd = "AB;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::B2A()
{
	cmd = "BA;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::swapAB()
{
	cmd = "SV;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

bool RIG_FTX_1::can_split()
{
	return true;
}

/// Split is done by moving the transmitter to the other side with FT.
/// ST1 is not used: on the FTX-1 it locks the SUB side in transmit.
void RIG_FTX_1::set_split(bool val)
{
	split = val;
	bool tx_on_sub = (inuse == onA);
	if (!val) {
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

int RIG_FTX_1::get_split()
{
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

int RIG_FTX_1::get_smeter()
{
	cmd = "SM0;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("SM");
	if (p == std::string::npos) return 0;
	if (p + 6 >= replystr.length()) return 0;
	int mtr = atoi(&replystr[p+3]);
	mtr = mtr / 2.56;
	return mtr;
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

int RIG_FTX_1::get_swr()
{
	return (int)ceil(get_meter('6') / 2.56);
}

int RIG_FTX_1::get_alc()
{
	return (int)ceil(get_meter('4') / 2.56);
}

// RM5 (PO) to watts.  PROVISIONAL: these are not measured on an FTX-1.
// The SPA-1 table is the FT-891 100 W curve and the field head table is
// the same curve scaled to 10 W.  They need to be replaced with readings
// taken against a wattmeter.
static meterpair pwrtbl_spa1[] = {
{0, 0},
{35, 5.0},
{59, 10.0},
{72, 15.0},
{86, 20.0},
{96, 25.0},
{107, 30.0},
{119, 35.0},
{129, 40.0},
{141, 45.0},
{150, 50.0},
{155, 55.0},
{161, 60.0},
{166, 65.0},
{173, 70.0},
{179, 75.0},
{184, 80.0},
{191, 85.0},
{196, 90.0},
{202, 95.0},
{208, 100.0}
};

static meterpair pwrtbl_field[] = {
{0, 0},
{35, 0.5},
{59, 1.0},
{72, 1.5},
{86, 2.0},
{96, 2.5},
{107, 3.0},
{119, 3.5},
{129, 4.0},
{141, 4.5},
{150, 5.0},
{155, 5.5},
{161, 6.0},
{166, 6.5},
{173, 7.0},
{179, 7.5},
{184, 8.0},
{191, 8.5},
{196, 9.0},
{202, 9.5},
{208, 10.0}
};

int RIG_FTX_1::get_power_out()
{
	int mtr = get_meter('5');

	meterpair *pwrtbl = pwrtbl_spa1;
	size_t entries = sizeof(pwrtbl_spa1) / sizeof(*pwrtbl_spa1);
	if (m_head == '1') {
		pwrtbl = pwrtbl_field;
		entries = sizeof(pwrtbl_field) / sizeof(*pwrtbl_field);
	}

	size_t i = 0;
	for (i = 0; i < entries - 1; i++) {
		if (mtr >= pwrtbl[i].mtr && mtr < pwrtbl[i + 1].mtr) {
			break;
		}
	}
	if (i == entries - 1) {
		return (int)ceil(pwrtbl[i].val);
	}
	int val = (int)ceil(
		pwrtbl[i].val +
		(pwrtbl[i + 1].val - pwrtbl[i].val) * (mtr - pwrtbl[i].mtr) /
		(pwrtbl[i + 1].mtr - pwrtbl[i].mtr));

	return val;
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

void RIG_FTX_1::get_pc_min_max_step(double &min, double &max, double &step)
{
	if (m_head == '0') {
		read_head();
	}
	if (m_head == '1') {
		min = 0.5; max = 10; step = 0.5;
	} else {
		min = 5; max = 100; step = 1;
	}
	pmax = max;
}

double RIG_FTX_1::get_power_control()
{
	read_head();

	size_t pos = replystr.rfind("PC");
	if (pos == std::string::npos || pos + 3 >= replystr.length()) {
		return progStatus.power_level;
	}
	return atof(replystr.substr(pos + 3).c_str());
}

void RIG_FTX_1::set_power_control(double val)
{
	if (m_head == '0') {
		read_head();
	}
	char cmdstr[20];
	if (m_head == '1') {
		if (val < 0.5) {
			val = 0.5;
		}
		if (val > 10) {
			val = 10;
		}
		int tenths = (int)round(val * 10);
		if (tenths % 10 == 0) {
			snprintf(cmdstr, sizeof(cmdstr), "PC1%03d;", tenths / 10);
		} else {
			snprintf(cmdstr, sizeof(cmdstr), "PC1%.1f;", tenths / 10.0);
		}
	} else if (m_head == '2') {
		if (val < 5) {
			val = 5;
		}
		if (val > 100) {
			val = 100;
		}
		snprintf(cmdstr, sizeof(cmdstr), "PC2%03d;", (int)round(val));
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

// Volume control return 0 ... 100
int RIG_FTX_1::get_volume_control()
{
	cmd = "AG0;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("AG");
	if (p == std::string::npos) return progStatus.volume;
	if (p + 6 >= replystr.length()) return progStatus.volume;
	int val = round(atoi(&replystr[p+3]) / 2.55);
	if (val > 100) val = 100;
	return ceil(val);
}

void RIG_FTX_1::set_volume_control(int val)
{
	int ivol = (int)(val * 2.55);
	cmd = "AG0000;";
	cmd[2] = active_side();
	for (int i = 5; i > 2; i--) {
		cmd[i] += ivol % 10;
		ivol /= 10;
	}

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

// Tranceiver PTT on/off
void RIG_FTX_1::set_PTT_control(int val)
{
	cmd = val ? "TX1;" : "TX0;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
	ptt_ = val;
}

int RIG_FTX_1::get_PTT()
{
	cmd = "TX;";

	get_trace(1, __func__);
	wait_char(';', 4, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("TX");
	if (p == std::string::npos) return ptt_;
	ptt_ =  (replystr[p+2] != '0' ? 1 : 0);

	return ptt_;
}


// AC P1 P2 P3
//   P1 0 internal tuner, 1 external tuner (flrig "external tuner")
//   P2 0 antenna tuner
//   P3 0 off / stop, 1 on, 3 start tuning
void RIG_FTX_1::tune_rig(int how)
{
	switch (how) {
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
	if (how != 0 && progStatus.external_tuner) {
		cmd[2] = '1';
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_tune()
{
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

void RIG_FTX_1::set_attenuator(int val)
{
	if (val) cmd = "RA01;";
	else     cmd = "RA00;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_attenuator()
{
	cmd = "RA0;";
	get_trace(1, __func__);
	wait_char(';', 5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("RA");
	if (p == std::string::npos) return progStatus.attenuator;
	if (p + 3 >= replystr.length()) return progStatus.attenuator;
	atten_level = replystr[p+3] - '0';

	return atten_level;
}

/// PA P1 for the active side's frequency: '0' HF/50, '1' 144, '2' 430
char RIG_FTX_1::preamp_band() {
	unsigned long long freq = freqA;
	if (inuse == onB) {
		freq = freqB;
	}
	if (freq >= 420000000ULL) {
		return '2';
	}
	if (freq >= 144000000ULL) {
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
		pre_labels_ = FTX_1_pre_labels;
	} else {
		pre_labels_ = FTX_1_pre_labels_vu;
	}
}

int RIG_FTX_1::next_preamp()
{
	if (preamp_state >= preamp_max(preamp_band())) {
		return 0;
	}
	return preamp_state + 1;
}

void RIG_FTX_1::set_preamp(int val)
{
	char band = preamp_band();
	if (val < 0) {
		val = 0;
	}
	if (val > preamp_max(band)) {
		val = preamp_max(band);
	}
	preamp_state = val;
	preamp_labels(band);

	cmd = "PA00;";
	cmd[2] = band;
	cmd[3] = '0' + val;
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_preamp()
{
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

int RIG_FTX_1::adjust_bandwidth(int val)
{
	bandwidths_ = bwtable(val);
	switch (val) {
		case mCW_U:
		case mCW_L:
		case mRTTYL:
		case mRTTYU:
			bw_vals_ = FTX_1_wvals_CW;
			break;
		case mDATAL:
		case mDATAU:
		case mPSK:
			bw_vals_ = FTX_1_wvals_DATA;
			break;
		case mAM:
		case mAMN:
			bw_vals_ = FTX_1_wvals_AM;
			break;
		case mFMN:
		case mDATAFMN:
			bw_vals_ = FTX_1_wvals_FMN;
			break;
		case mFM:
		case mDATAFM:
		case mC4FMDN:
		case mC4FMVW:
			bw_vals_ = FTX_1_wvals_FM;
			break;
		default:
			bw_vals_ = FTX_1_wvals_SSB;
	}
	return FTX_1_def_bw[val];
}

int RIG_FTX_1::def_bandwidth(int val)
{
	return FTX_1_def_bw[val];
}

std::vector<std::string>& RIG_FTX_1::bwtable(int n)
{
	switch (n) {
		case mCW_U:
		case mCW_L:
		case mRTTYL:
		case mRTTYU:
			return FTX_1_widths_CW;
		case mDATAL:
		case mDATAU:
		case mPSK:
			return FTX_1_widths_DATA;
		case mAM:
		case mAMN:
			return FTX_1_widths_AM;
		case mFMN:
		case mDATAFMN:
			return FTX_1_widths_FMN;
		case mFM:
		case mDATAFM:
		case mC4FMDN:
		case mC4FMVW:
			return FTX_1_widths_FM;
		default:
			return FTX_1_widths_SSB;
	}
}

/// Set the width for one side.  NARROW is turned off first because with
/// NARROW on the FTX-1 uses the menu NAR WIDTH and the SH setting does not
/// hold.
void RIG_FTX_1::set_width(char side, int bw_index) {
	cmd = "NA00;";
	cmd[2] = side;
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

	cmd = "SH00";
	cmd[2] = side;
	cmd += '0' + bw_index / 10;
	cmd += '0' + bw_index % 10;
	cmd += ';';
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::set_modeA(int val)
{
	modeA = val;

	adjust_bandwidth(modeA);

	cmd = "MD0";
	cmd += FTX_1_mode_chr[val];
	cmd += ';';

	set_trace(1, "set_modeA()");
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, "SET mode A", cmd, replystr);

}

int RIG_FTX_1::get_modeA()
{
	cmd = "MD0;";
	get_trace(1, __func__);
	wait_char(';',5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("MD");
	if (p != std::string::npos) {
		if (p + 3 < replystr.length()) {
			int md = 0;
			switch (replystr[p+3]) {
				case '1': md = mLSB; break;
				case '2': md = mUSB; break;
				case '3': md = mCW_U; break;
				case '4': md = mFM; break;
				case '5': md = mAM; break;
				case '6': md = mRTTYL; break;
				case '7': md = mCW_L; break;
				case '8': md = mDATAL; break;
				case '9': md = mRTTYU; break;
				case 'A': md = mDATAFM; break;
				case 'B': md = mFMN; break;
				case 'C': md = mDATAU; break;
				case 'D': md = mAMN; break;
				case 'E': md = mPSK; break;
				case 'F': md = mDATAFMN; break;
				case 'H': md = mC4FMDN; break;
				case 'I': md = mC4FMVW; break;
			}
			modeA = md;
		}
	}

	adjust_bandwidth(modeA);

	return modeA;
}

void RIG_FTX_1::set_modeB(int val)
{
	modeB = val;

	adjust_bandwidth(modeB);

	cmd = "MD1";
	cmd += FTX_1_mode_chr[val];
	cmd += ';';

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

}

int RIG_FTX_1::get_modeB()
{

	cmd = "MD1;";
	get_trace(1, __func__);
	wait_char(';',5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("MD");
	if (p != std::string::npos) {
		if (p + 3 < replystr.length()) {
			int md = 0;
			switch (replystr[p+3]) {
				case '1': md = mLSB; break;
				case '2': md = mUSB; break;
				case '3': md = mCW_U; break;
				case '4': md = mFM; break;
				case '5': md = mAM; break;
				case '6': md = mRTTYL; break;
				case '7': md = mCW_L; break;
				case '8': md = mDATAL; break;
				case '9': md = mRTTYU; break;
				case 'A': md = mDATAFM; break;
				case 'B': md = mFMN; break;
				case 'C': md = mDATAU; break;
				case 'D': md = mAMN; break;
				case 'E': md = mPSK; break;
				case 'F': md = mDATAFMN; break;
				case 'H': md = mC4FMDN; break;
				case 'I': md = mC4FMVW; break;
			}
			modeB = md;
		}
	}

	adjust_bandwidth(modeB);
	return modeB;
}

void RIG_FTX_1::set_bwA(int val)
{
	bwA = val;
	if (fixed_width(modeA)) {
		return;
	}
	set_width('0', bw_vals_[val]);
}

int RIG_FTX_1::get_bwA()
{
	size_t p;

	if (fixed_width(modeA)) {
		bwA = 0;
		return bwA;
	}

	cmd = "SH0;";

	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

// response should be similar to
//   0123456
//   SH00nn;
//   nn = filter number 

	p = replystr.rfind("SH00");
	if (p == std::string::npos) return bwA;
	if (p + 6 >= replystr.length()) return bwA;

	int filnbr = 0;
	sscanf(&replystr[p], "SH00%d;", &filnbr);

	const int *idx = bw_vals_;
	int i = 0;
	while (*idx != WVALS_LIMIT) {
		if (*idx == filnbr) break;
		idx++;
		i++;
	}
	if (*idx == WVALS_LIMIT) i--;
	bwA = i;

	return bwA;
}

void RIG_FTX_1::set_bwB(int val)
{
	bwB = val;
	if (fixed_width(modeB)) {
		return;
	}
	set_width('1', bw_vals_[val]);
}

int RIG_FTX_1::get_bwB()
{
	size_t p;

	if (fixed_width(modeB)) {
		bwB = 0;
		return bwB;
	}
	cmd = "SH1;";
	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	p = replystr.rfind("SH10");
	if (p == std::string::npos) return bwB;
	if (p + 6 >= replystr.length()) return bwB;

	int filnbr = 0;
	sscanf(&replystr[p], "SH10%d;", &filnbr);

	const int *idx = bw_vals_;
	int i = 0;
	while (*idx != WVALS_LIMIT) {
		if (*idx == filnbr) break;
		idx++;
		i++;
	}
	if (*idx == WVALS_LIMIT) i--;
	bwB = i;

	return bwB;
}

int RIG_FTX_1::get_modetype(int n)
{
	return FTX_1_mode_type[n];
}

void RIG_FTX_1::set_if_shift(int val)
{
	char cmdstr[20];
	snprintf(cmdstr, sizeof(cmdstr), "IS%c0%+05d;", active_side(), val);
	cmd = cmdstr;

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

bool RIG_FTX_1::get_if_shift(int &val)
{
	if (inuse == onA)
		cmd = "IS0;";
	else
		cmd = "IS1;";
	get_trace(1, __func__);
	wait_char(';',10, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("IS");
	if (p == std::string::npos) {
		val = progStatus.shift_val;
		return true;
	}

	char sub = '0';
	int sh = progStatus.shift_val;
	sscanf(&replystr[p], "IS%c0%d;", &sub, &sh);

	val = sh;
	return true;
}

void RIG_FTX_1::set_notch(bool on, int val)
{
// set notch frequency
	if (on) {
		cmd = "BP00001;";
		cmd[2] = active_side();
		set_trace(1, __func__);
		sendCommand(cmd);
		sett("");
		showresp(WARN, ASC, __func__, cmd, replystr);
		cmd = "BP01000;";
		cmd[2] = active_side();
		if (val % 10 >= 5) val += 10;
		val /= 10;
		for (int i = 3; i > 0; i--) {
			cmd[3 + i] += val % 10;
			val /=10;
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

bool  RIG_FTX_1::get_notch(int &val)
{
	bool ison = false;
	cmd = "BP00;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';',8, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("BP");
	if (p == std::string::npos || p + 6 >= replystr.length()) {
		return ison;
	}

	if (replystr[p+6] == '1') { // manual notch enabled
		ison = true;
		val = progStatus.notch_val;
		cmd = "BP01;";
		cmd[2] = active_side();
		get_trace(1, "get notch value()");
		wait_char(';',8, FTX_1_WAIT_TIME, "get notch val", ASC);
		gett("");
		p = replystr.rfind("BP");
		if (p != std::string::npos && p + 6 < replystr.length()) {
			val = fm_decimal(replystr.substr(p+4), 3) * 10;
		}
	}
	return ison;
}

void RIG_FTX_1::set_auto_notch(int v)
{
	cmd.assign("BC").append(1, active_side());
	cmd.append(v ? "1" : "0").append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int  RIG_FTX_1::get_auto_notch()
{
	cmd = "BC0;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';',5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");
	size_t p = replystr.rfind("BC");
	if (p == std::string::npos) return 0;
	if (replystr[p+3] == '1') return 1;
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

// noise blanker on/off
void RIG_FTX_1::set_noise(bool on)
{
	m_nb_on = on;
	set_dsp_level("NL", 3, on ? m_nb_level : 0);
}

int RIG_FTX_1::get_noise()
{
	read_dsp("NL", 3, m_nb_on, m_nb_level);
	return m_nb_on;
}

void RIG_FTX_1::set_nb_level(int val)
{
	m_nb_level = dsp_level(val);
	if (m_nb_on) {
		set_dsp_level("NL", 3, m_nb_level);
	}
}

int RIG_FTX_1::get_nb_level()
{
	read_dsp("NL", 3, m_nb_on, m_nb_level);
	return m_nb_level;
}

// noise reduction (DNR) on/off
void RIG_FTX_1::set_noise_reduction(int val)
{
	m_nr_on = (val != 0);
	set_dsp_level("RL", 2, m_nr_on ? m_nr_level : 0);
}

int  RIG_FTX_1::get_noise_reduction()
{
	read_dsp("RL", 2, m_nr_on, m_nr_level);
	return m_nr_on;
}

void RIG_FTX_1::set_noise_reduction_val(int val)
{
	m_nr_level = dsp_level(val);
	if (m_nr_on) {
		set_dsp_level("RL", 2, m_nr_level);
	}
}

int  RIG_FTX_1::get_noise_reduction_val()
{
	read_dsp("RL", 2, m_nr_on, m_nr_level);
	return m_nr_level;
}

// val 0 .. 100
void RIG_FTX_1::set_mic_gain(int val)
{
	cmd = "MG000;";
	for (int i = 4; i > 1; i--) {
		cmd[i] = val % 10 + '0';
		val /= 10;
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_mic_gain()
{
	cmd = "MG;";
	get_trace(1, __func__);
	wait_char(';',6, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("MG");
	if (p == std::string::npos) return progStatus.mic_gain;
	int val = atoi(&replystr[p+2]);
	if (val > 100) val = 100;
	return ceil(val);
}

// RG P1 000 - 255, flrig slider 0 - 100
void RIG_FTX_1::set_rf_gain(int val)
{
	int rf_level = round(val * 2.55);
	cmd.assign("RG").append(1, active_side());
	cmd.append(to_decimal(rf_level, 3)).append(";");

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int  RIG_FTX_1::get_rf_gain()
{
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

void RIG_FTX_1::set_squelch(int val)
{
	char cmdstr[12];
	snprintf(cmdstr, sizeof(cmdstr), "%c%03d",
		(inuse == onA ? '0' : '1'), val * 255 / 100);
	cmd.assign("SQ").append(cmdstr).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int  RIG_FTX_1::get_squelch()
{
	if (inuse == onA)
		cmd = "SQ0;";
	else
		cmd = "SQ1;";
	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("SQ");
	if (p == std::string::npos) {
		return progStatus.squelch;
	}
	char sub_side = '0';
	int level = 0;
	sscanf(&replystr[p], "SQ%c%d;", &sub_side, &level);

	return level * 100 / 255;
}


// VX     VOX on/off
// VG     VOX GAIN 000 - 100
// VD     VOX DELAY, 2 digit code (see FTX_1_DELAY_MSEC)
// EX030510 VOX SELECT 0: MIC, 1: USB, 2: Bluetooth
// VG and VD apply to the input chosen by VOX SELECT.
// The FTX-1 has no anti-VOX setting available over CAT.

void RIG_FTX_1::set_vox_onoff()
{
	cmd = "VX0;";
	if (progStatus.vox_onoff) cmd[2] = '1';
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::set_vox_gain()
{
	cmd.assign("VG").append(to_decimal(progStatus.vox_gain, 3)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::set_vox_hang()
{
	cmd.assign("VD").append(to_decimal(delay_code(progStatus.vox_hang), 2));
	cmd.append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::set_vox_on_dataport()
{
	cmd = "EX0305100;";
	if (progStatus.vox_on_dataport) {
		cmd[8] = '1';
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::set_cw_wpm()
{
	cmd = "KS";
	if (progStatus.cw_wpm > 60) progStatus.cw_wpm = 60;
	if (progStatus.cw_wpm < 4) progStatus.cw_wpm = 4;
	cmd.append(to_decimal(progStatus.cw_wpm, 3)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

// ML0 monitor on/off: 000 off, 001 on
// ML1 monitor level 000 - 100
void RIG_FTX_1::set_cw_vol()
{
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

int  RIG_FTX_1::get_cw_vol()
{
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

void RIG_FTX_1::enable_keyer()
{
	if (progStatus.enable_keyer)
		cmd = "KR1;";
	else
		cmd = "KR0;";
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_keyer()
{
	cmd = "KR;";
	get_trace(1, __func__);
	wait_char(';', 4, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");
	size_t p = replystr.rfind("KR");
	if (p != std::string::npos) return replystr[p+2] - '0';
	return 0;
}

bool RIG_FTX_1::set_cw_spot()
{
	if (vfo->imode == mCW_U || vfo->imode == mCW_L) {
		cmd = "CS0;";
		if (progStatus.spot_onoff) cmd[2] = '1';
		set_trace(1, __func__);
		sendCommand(cmd);
		sett("");
		showresp(WARN, ASC, __func__, cmd, replystr);
		return true;
	} else
		return false;
}

// EX020203 CW WEIGHT 25 - 45 (2.5 - 4.5)
void RIG_FTX_1::set_cw_weight()
{
	int weight = round(progStatus.cw_weight * 10);
	cmd.assign("EX020203").append(to_decimal(weight, 2)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

// EX020117 QSK DELAY TIME 0: 15, 1: 20, 2: 25, 3: 30 msec
void RIG_FTX_1::set_cw_qsk()
{
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

int  RIG_FTX_1::get_cw_qsk()
{
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

// SD CW break-in delay, 2 digit code
void RIG_FTX_1::set_cw_delay()
{
	cmd.assign("SD").append(to_decimal(delay_code(progStatus.cw_delay), 2));
	cmd.append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int  RIG_FTX_1::get_cw_delay()
{
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

// 00: 300 Hz to 75: 1050 Hz (10Hz steps)
void RIG_FTX_1::set_cw_spot_tone()
{
	int n = progStatus.cw_spot_tone / 10 - 30;
	if (n < 0) n = 0;
	if (n > 75) n = 75;
	cmd.assign("KP").append(to_decimal(n, 2)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/*
 * auto on/off not working well needs much work
*/

void RIG_FTX_1::set_xcvr_auto_on()
{
	if (!progStatus.xcvr_auto_on) return;

// This command requires dummy data be initially sent. Then after one
// second and before two seconds the command is sent.

	cmd = "PS1;"; // use as the dummy data
	sendCommand(cmd);
	update_progress(0);
	for (int i = 0; i < 1200; i += 100) {
		MilliSleep(100);
		update_progress(100 * i / 6000);
		Fl::awake();
	}

	cmd = "PS;";
	get_trace(1, "xcvr ON? ()");
	wait_char(';',4, 500, "Test: Is Rig ON", ASC);
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

void RIG_FTX_1::set_xcvr_auto_off()
{
	if (!progStatus.xcvr_auto_off) return;

	cmd = "PS0;";
	set_trace(1, "set_xcvr_OFF()");
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, "SET xcvr auto on/off", cmd, replystr);
}


void RIG_FTX_1::set_compression(int on, int val)
{
	cmd = "PL";
	cmd.append(to_decimal(val, 3)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

	// PR0 speech processor, 1 off, 2 on.  Only sent in SSB, where the
	// processor applies.
	int curMode = rigbase::isOnA() ? modeA : modeB;
	if ( curMode == mLSB || curMode == mUSB ) {
		if (on) {
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

void RIG_FTX_1::get_compression(int &on, int &val)
{ 
	on = 0; val = 0;

	cmd = "PL;";
	get_trace(1, __func__);
	wait_char(';',6, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("PL");
	if (p == std::string::npos) return;
	val = atoi(&replystr[p+2]);
	if (val > 100) val = 100;
	val = ceil(val);

	// PR0 speech processor, 1 off, 2 on.  Only read in SSB.
	int curMode = rigbase::isOnA() ? modeA : modeB;
	if ( curMode == mLSB || curMode == mUSB ) {
		cmd = "PR0;";
		get_trace(1, "get comp level");
		wait_char(';', 7, FTX_1_WAIT_TIME, "get PR level", ASC);
		gett("");
		size_t p = replystr.rfind("PR0");
		if (p == std::string::npos) return;

		on = (replystr[p+3] == '2');
	}
	
	std::stringstream s;
	s << "get_compression: " << (on ? "ON" : "OFF") << "(" << on << "), comp PL=" << val;
	get_trace(1, s.str().c_str());
}


// Band buttons 1 - 13 (1.8 ... 50, 144, 430, Gen) to BS band codes
//   00 1.8  01 3.5  02 5  03 7  04 10  05 14  06 18  07 21  08 24.5
//   09 28  10 50  11 70/GEN  13 144  14 430
void RIG_FTX_1::get_band_selection(int v)
{
	static const char *BAND_CODES[] = {
		"00", "01", "03", "04", "05", "06", "07", "08", "09", "10",
		"13", "14", "11" };
	if (v < 1 || v > 13) {
		return;
	}
	cmd.assign("BS").append(1, active_side());
	cmd.append(BAND_CODES[v - 1]).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

// EX030113 REF FREQ ADJ -25 - +25, always signed: +05, -12
void RIG_FTX_1::setVfoAdj(double v)
{
	char cmdstr[20];
	snprintf(cmdstr, sizeof(cmdstr), "EX030113%+03d;", (int)v);
	cmd = cmdstr;
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

double RIG_FTX_1::getVfoAdj()
{
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

void RIG_FTX_1::get_vfoadj_min_max_step(double &min, double &max, double &step)
{
	min = -25;
	max = 25;
	step = 1;
}

//----------------------------------------------------------------------
// AGC control
//----------------------------------------------------------------------

int  RIG_FTX_1::get_agc()
{
	cmd = "GT0;";
	cmd[2] = active_side();
	wait_char(';', 6, FTX_1_WAIT_TIME, __func__, ASC);
	gett(__func__);

	size_t p = replystr.rfind("GT");
	if (p == std::string::npos) return agcval;

	if (p + 3 >= replystr.length()) return agcval;
	switch (replystr[p + 3]) {
		default:
		case '0': agcval = 0; break;
		case '1': agcval = 1; break;
		case '2': agcval = 2; break;
		case '3': agcval = 3; break;
		case '4': case '5':
		case '6': agcval = 4; break;
	}
	return agcval;
}

int RIG_FTX_1::incr_agc()
{
static const char ch[] = {'0', '1', '2', '3', '4'};
	agcval++;
	if (agcval > 4) agcval = 0;
	cmd = "GT00;";
	cmd[2] = active_side();
	cmd[3] = ch[agcval];

	sendCommand(cmd);
	showresp(WARN, ASC, __func__, cmd, replystr);
	sett(__func__);

	return agcval;
}


static const char *agcstr[] = {"AGC", "FST", "MED", "SLO", "AUT"};
const char *RIG_FTX_1::agc_label()
{
	if (agcval < 0 || agcval > 4) return "AGC";
	return agcstr[agcval];
}

int  RIG_FTX_1::agc_val()
{
	return (agcval);
}
 
void  RIG_FTX_1::zero_in()
{
	cmd = "ZI0;";
	cmd[2] = active_side();
	sendCommand(cmd);
}

const char * RIG_FTX_1::get_bwname_(int bw, int md) {
// read bw based on mode
	try {
		std::string sbw = bwtable(md).at(bw);
		return (bwtable(md).at(bw)).c_str();
	} catch (const std::exception& e) {
		LOG_ERROR("%s", e.what());
	}
	return "";
}

