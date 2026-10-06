// ----------------------------------------------------------------------------
// Copyright (C) 2023
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

#include "yaesu/FT710.h"

#include <sstream>

#include "debug.h"
#include "support.h"

// mode index
enum mFT710 {
	mLSB, mUSB, mCW_U, mFM, mAM, mRTTY_L, mCW_L, mDATA_L, mRTTY_U,
	mDATA_FM, mFM_N, mDATA_U, mAM_N, mPSK, mDATA_FMN };

static const char FT710_NAME[] = "FT-710";

#undef  NUM_MODES
#define NUM_MODES  15

// default bandwidth index for each mode index, narrow
static const int FT710_DEF_BW_NARROW[NUM_MODES] = {
// LSB, USB, CW-U, FM, AM
	 6,  6,  9,  0,  0,
// RTTY-L, CW-L, DATA-L, RTTY-U, DATA-FM
	10,  9,  6, 10,  0,
// FM-N, DATA-U, AM-N, PSK, DATA-FMN
	 0,  6,  0,  5,  0 };

// default bandwidth index for each mode index, wide
static const int FT710_DEF_BW_WIDE[NUM_MODES] = {
// LSB, USB, CW-U, FM, AM
	13, 13, 16,  0,  0,
// RTTY-L, CW-L, DATA-L, RTTY-U, DATA-FM
	10, 16, 17, 10,  0,
// FM-N, DATA-U, AM-N, PSK, DATA-FMN
	 0, 17,  0,  9,  0 };

// bandwidth index last used in each mode, -1 if not yet used
static int mode_bwA[NUM_MODES] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
static int mode_bwB[NUM_MODES] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

static std::vector<std::string> ft710_modes;
static const char *FT710_MODE_NAMES[] = {
	"LSB",    "USB",    "CW-U",   "FM",     "AM",
	"RTTY-L", "CW-L",   "DATA-L", "RTTY-U", "DATA-FM",
	"FM-N",   "DATA-U", "AM-N",   "PSK",    "DATA-FMN" };

// MD P2 mode character for each mode index
static const char FT710_MODE_CHR[] = {
	'1', '2', '3', '4', '5',
	'6', '7', '8', '9', 'A',
	'B', 'C', 'D', 'E', 'F' };

static const char FT710_MODE_TYPE[] = {
	'L', 'U', 'U', 'U', 'U',
	'L', 'L', 'L', 'U', 'U',
	'U', 'U', 'U', 'U', 'U' };

// SSB widths, SH P3 codes 01 - 23
static std::vector<std::string> ft710_widths_ssb;
static const char *FT710_WIDTHS_SSB[] = {
	 "300",  "400",  "600",  "850", "1100",
	"1200", "1500", "1650", "1800", "1950",
	"2100", "2250", "2400", "2450", "2500",
	"2600", "2700", "2800", "2900", "3000",
	"3200", "3500", "4000" };

static const int FT710_WVALS_SSB[] = {
	 1,  2,  3,  4,  5,
	 6,  7,  8,  9, 10,
	11, 12, 13, 14, 15,
	16, 17, 18, 19, 20,
	21, 22, 23, WVALS_LIMIT };

// CW widths, SH P3 codes 01 - 21
static std::vector<std::string> ft710_widths_cw;
static const char *FT710_WIDTHS_CW[] = {
	  "50",  "100",  "150",  "200",  "250",
	 "300",  "350",  "400",  "450",  "500",
	 "600",  "800", "1200", "1400", "1700",
	"2000", "2400", "3000", "3200", "3500",
	"4000" };

static const int FT710_WVALS_CW[] = {
	 1,  2,  3,  4,  5,
	 6,  7,  8,  9, 10,
	11, 12, 13, 14, 15,
	16, 17, 18, 19, 20,
	21, WVALS_LIMIT };

// RTTY widths, SH P3 codes 01 - 21
static std::vector<std::string> ft710_widths_rtty;
static const char *FT710_WIDTHS_RTTY[] = {
	  "50",  "100",  "150",  "200",  "250",
	 "300",  "350",  "400",  "450",  "500",
	 "600",  "800", "1200", "1400", "1700",
	"2000", "2400", "3000", "3200", "3500",
	"4000" };

static const int FT710_WVALS_RTTY[] = {
	 1,  2,  3,  4,  5,
	 6,  7,  8,  9, 10,
	11, 12, 13, 14, 15,
	16, 17, 18, 19, 20,
	21, WVALS_LIMIT };

// DATA and PSK widths, SH P3 codes 01 - 21
static std::vector<std::string> ft710_widths_data;
static const char *FT710_WIDTHS_DATA[] = {
	  "50",  "100",  "150",  "200",  "250",
	 "300",  "350",  "400",  "450",  "500",
	 "600",  "800", "1200", "1400", "1700",
	"2000", "2400", "3000", "3200", "3500",
	"4000" };

static const int FT710_WVALS_PSK[] = {
	 1,  2,  3,  4,  5,
	 6,  7,  8,  9, 10,
	11, 12, 13, 14, 15,
	16, 17, 18, 19, 20,
	21, WVALS_LIMIT };

// Single bandwidth modes
static const int FT710_WVALS_AMFM[] = { 0, WVALS_LIMIT };

static std::vector<std::string> ft710_widths_am_wide;
static const char *FT710_WIDTHS_AM_WIDE[] = { "9000" };
static std::vector<std::string> ft710_widths_am_nar;
static const char *FT710_WIDTHS_AM_NAR[] = { "6000" };
static std::vector<std::string> ft710_widths_fm_nar;
static const char *FT710_WIDTHS_FM_NAR[] = { "9000" };
static std::vector<std::string> ft710_widths_fm_wide;
static const char *FT710_WIDTHS_FM_WIDE[] = { "16000" };
static std::vector<std::string> ft710_widths_data_fm;
static const char *FT710_WIDTHS_DATA_FM[] = { "16000" };
static std::vector<std::string> ft710_widths_data_fmn;
static const char *FT710_WIDTHS_DATA_FMN[] = { "9000" };

/// 60 m combo entries: VFO selects the 5 MHz band (BS02), the others
/// are the radio's 5 MHz memory channels, MC 5xx (manual p.15).  A US
/// FT-710 has 501-505 at 5330.5, 5346.5, 5357.0, 5371.5 and 5403.5 kHz
/// USB, and 506-510 at the same channels 1.5 kHz up in CW.  The radio
/// holds the frequencies, so only the channel numbers are listed here.
static std::vector<std::string> ft710_60m;
static const char *FT710_60M[] = {
	"VFO", "501", "502", "503", "504", "505",
	"506", "507", "508", "509", "510"
};

//----------------------------------------------------------------------
static std::vector<std::string> ft710_att_labels;
static const char *FT710_ATT_LABELS[] = { "ATT", "6 dB", "12 dB", "18 dB" };

static std::vector<std::string> ft710_pre_labels;
static const char *FT710_PRE_LABELS[] = { "IPO", "Amp 1", "Amp 2" };

static std::vector<std::string> ft710_nb_labels;
static const char *FT710_NB_LABELS[] = { "NB", "NB on" };
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

/// build the tables, turn Auto Information off and set up the 60 m combo
void RIG_FT710::initialize() {
	name_ = FT710_NAME;

	VECTOR(ft710_modes, FT710_MODE_NAMES);
	VECTOR(ft710_widths_ssb, FT710_WIDTHS_SSB);
	VECTOR(ft710_widths_cw, FT710_WIDTHS_CW);
	VECTOR(ft710_widths_rtty, FT710_WIDTHS_RTTY);
	VECTOR(ft710_widths_data, FT710_WIDTHS_DATA);
	VECTOR(ft710_widths_am_wide, FT710_WIDTHS_AM_WIDE);
	VECTOR(ft710_widths_am_nar, FT710_WIDTHS_AM_NAR);
	VECTOR(ft710_widths_fm_nar, FT710_WIDTHS_FM_NAR);
	VECTOR(ft710_widths_fm_wide, FT710_WIDTHS_FM_WIDE);
	VECTOR(ft710_widths_data_fm, FT710_WIDTHS_DATA_FM);
	VECTOR(ft710_widths_data_fmn, FT710_WIDTHS_DATA_FMN);
	VECTOR(ft710_60m, FT710_60M);

	VECTOR(ft710_att_labels, FT710_ATT_LABELS);
	att_labels_ = ft710_att_labels;

	VECTOR(ft710_pre_labels, FT710_PRE_LABELS);
	pre_labels_ = ft710_pre_labels;

	VECTOR(ft710_nb_labels, FT710_NB_LABELS);
	nb_labels_ = ft710_nb_labels;

	modes_ = ft710_modes;
	bandwidths_ = ft710_widths_ssb;
	bw_vals_ = FT710_WVALS_SSB;

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

	cmd = "AI0;";
	sendCommand(cmd);
	showresp(WARN, ASC, "Auto Info OFF", cmd, replystr);
	sett("Auto Info OFF");

	set_cw_spot();

	op_yaesu_select60->clear();
	for (size_t entry = 0; entry < ft710_60m.size(); entry++) {
		op_yaesu_select60->add(ft710_60m[entry].c_str());
	}
	op_yaesu_select60->index(m_60m_indx);
	op_yaesu_select60->activate();

	get_vfoAorB();
}

/// base class values and the controls the FT-710 supports
RIG_FT710::RIG_FT710() {
// base class values
	IDstr = "ID";
	name_ = FT710_NAME;
	modes_ = ft710_modes;
	bandwidths_ = ft710_widths_ssb;
	bw_vals_ = FT710_WVALS_SSB;

	widgets = rig_widgets;

	serial_baudrate = BR38400;
	stopbits = 1;
	serial_retries = 2;

	serial_write_delay = 0;
	serial_post_write_delay = 0;

	serial_timeout = 50;
	serial_rtscts = true;
	serial_rtsplus = false;
	serial_dtrplus = false;
	serial_catptt = true;
	serial_rtsptt = false;
	serial_dtrptt = false;

	A.imode = B.imode = modeB = modeA = def_mode = 1;
	A.iBW = B.iBW = bwA = bwB = def_bw = 0;
	A.freq = B.freq = freqA = freqB = def_freq = 14070000ULL;

	has_band_selection =
	has_extras =
	has_vox_onoff =
	has_vox_gain =
	has_vox_anti =
	has_vox_hang =
	has_vox_on_dataport =

	has_cw_wpm =
	has_cw_keyer =
	has_cw_spot =
	has_cw_qsk =
	has_cw_weight =
	has_cw_break_in =
	has_split =
	can_change_alt_vfo =
	has_smeter =
	has_swr_control =
	has_alc_control =

	has_idd_control =
	has_voltmeter =

	has_power_out =
	has_power_control =
	has_volume_control =
	has_rf_control =
	has_sql_control =
	has_micgain_control =
	has_mode_control =
	has_noise_control =
	has_noise_reduction =
	has_noise_reduction_control =
	has_bandwidth_control =
	has_notch_control =
	has_auto_notch =
	has_attenuator_control =
	has_preamp_control =
	has_ifshift_control =
	has_ptt_control =
	has_tune_control =
	has_xcvr_auto_on_off = true;

// derived specific
	atten_state = 0;
	preamp_state = 0;
	m_notch_on = false;
	m_60m_indx = 0;

	for (int meter = 0; meter < 9; meter++) {
		m_meter_raw[meter] = 0;
	}

	inuse = onA;

	can_synch_clock = true;

	precision = 1;
	ndigits = 8;
}

/// PS1, turn the radio on unless it already answers ID
void RIG_FT710::set_xcvr_auto_on() {
	cmd = "ID;";
	wait_char(';', 7, 100, "check", ASC);
	if (replystr.find("ID") != std::string::npos) {
		return;
	}

// wait 1.2 seconds
	for (int i = 0; i < 12; i++) {
		MilliSleep(100);
		update_progress(i * 10);
		Fl::awake();
	}

	sendCommand("PS1;");
	sett("set xcvr auto ON");

	update_progress(0);

// wait up to 10 seconds for normal response
	cmd = "PS;";
	for (int i = 0; i < 100; i++) {
		wait_char(';', 4, 100, "Test for xcvr ON", ASC);
		if (replystr.find("PS1;") != std::string::npos) {
			update_progress(100);
			break;
		}
		update_progress(i);
		Fl::awake();
	}
}

/// PS0, turn the radio off
void RIG_FT710::set_xcvr_auto_off() {
	sendCommand("PS0;");
	sett("set_xcvr_auto_off");

// transceiver does not respond after a power OFF
	for (int i = 0; i < 100; i++) {
		cmd = "PS;";
		wait_char(';', 4, 100, "Test for xcvr OFF", ASC);
		if (replystr.empty()) {
			break;
		}
		Fl::awake();
	}
}

/// Band buttons 1-11 (1.8 MHz to GEN) select band stacks BS00-BS11,
/// skipping BS02; the 60 m combo calls with 13.
void RIG_FT710::get_band_selection(int band) {
	cmd = "IF;";
	wait_char(';', 28, 100, "get band", ASC);

	sett("get band");

	size_t pos = last_frame("IF", 28);
	if (pos == std::string::npos) {
		return;
	}
// IF P7 is 0 for VFO, otherwise a memory channel is in use
	if (replystr[pos + 22] != '0') {
		cmd = "VM;";
		sendCommand(cmd);
		showresp(WARN, ASC, "VFO mode", cmd, replystr);
	}

	if (band == 13) {
		m_60m_indx = op_yaesu_select60->index();
		if (m_60m_indx > 0 && m_60m_indx < (int)ft710_60m.size()) {
			cmd.assign("MC").append(ft710_60m[m_60m_indx]).append(";");
		} else {
			cmd = "BS02;";
		}
	} else {
		if (band < 3) {
			band = band - 1;
		}
		cmd.assign("BS").append(to_decimal(band, 2)).append(";");
	}

	sendCommand(cmd);
	showresp(WARN, ASC, "Select Band Stacks", cmd, replystr);
}

/// true if the radio answers ID
bool RIG_FT710::check() {
	cmd = "ID;";
	wait_char(';', 7, 500, "check", ASC);

	if (replystr.find("ID") == std::string::npos) {
		return false;
	}
	return true;
}

/// Position of the last complete reply in replystr that starts with
/// prefix and is length characters long, ';' included, or npos.  A
/// reply left over from an earlier command is never parsed as this
/// command's reply.
size_t RIG_FT710::last_frame(const char *prefix, size_t length) {
	size_t pos = replystr.rfind(prefix);
	if (pos == std::string::npos || pos + length > replystr.length() ||
		replystr[pos + length - 1] != ';') {
/// an empty reply is already logged by wait_char as a timeout
		if (!replystr.empty()) {
			LOG_WARN("no %s reply in \"%s\"", prefix, replystr.c_str());
		}
		return std::string::npos;
	}
	return pos;
}

/// FA, VFO A frequency
unsigned long long RIG_FT710::get_vfoA() {
	cmd = "FA";
	cmd += ';';
	wait_char(';', 12, 100, "get vfo A", ASC);
	gett("get_vfoA()");

	size_t pos = last_frame("FA", 12);
	if (pos == std::string::npos) {
		return freqA;
	}

	unsigned long long frequency = 0;

	sscanf(&replystr[pos], "FA%lld", &frequency);
	if (frequency) {
		freqA = frequency;
	}
	return freqA;
}

/// FA, VFO A frequency
void RIG_FT710::set_vfoA(unsigned long long frequency) {
	freqA = frequency;
	cmd = "FA000000000;";
	for (int i = 10; i > 1; i--) {
		cmd[i] += frequency % 10;
		frequency /= 10;
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "SET vfo A", cmd, replystr);
	sett("SET vfo A");
}

/// FB, VFO B frequency
unsigned long long RIG_FT710::get_vfoB() {
	cmd = rsp = "FB";
	cmd += ';';
	wait_char(';', 12, 100, "get vfo B", ASC);
	gett("get_vfoB()");

	size_t pos = last_frame("FB", 12);
	if (pos == std::string::npos) {
		return freqB;
	}

	unsigned long long frequency = 0;
	sscanf(&replystr[pos], "FB%lld", &frequency);
	if (frequency) {
		freqB = frequency;
	}
	return freqB;
}

/// FB, VFO B frequency
void RIG_FT710::set_vfoB(unsigned long long frequency) {
	freqB = frequency;
	cmd = "FB000000000;";
	for (int i = 10; i > 1; i--) {
		cmd[i] += frequency % 10;
		frequency /= 10;
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "SET vfo B", cmd, replystr);
	sett("SET vfo B");
}

/// the FT-710 has VFO A and VFO B
bool RIG_FT710::twovfos() {
	return true;
}

/// VS, the VFO in use
int RIG_FT710::get_vfoAorB() {
	cmd = "VS;";
	rsp = "VS";
	wait_char(';', 4, 100, "get vfoAorB()", ASC);
	gett("get vfoAorB()");
	size_t pos = replystr.rfind(rsp);
	if (pos != std::string::npos) {
		inuse = (replystr[pos + 2] == '1') ? onB : onA;
	}
	return inuse;
}

/// VS0, use VFO A
void RIG_FT710::selectA() {
	cmd = "VS0;";
	sendCommand(cmd);
	showresp(WARN, ASC, "select A", cmd, replystr);
	sett("selectA()");
	inuse = onA;
}

/// VS1, use VFO B
void RIG_FT710::selectB() {
	cmd = "VS1;";
	sendCommand(cmd);
	showresp(WARN, ASC, "select B", cmd, replystr);
	sett("selectB()");
	inuse = onB;
}

/// AB, copy VFO A to VFO B
void RIG_FT710::A2B() {
	cmd = "AB;";
	sendCommand(cmd);
	showresp(WARN, ASC, "vfo A --> B", cmd, replystr);
	sett("A2B()");
}

/// split is available
bool RIG_FT710::can_split() {
	return true;
}

/// ST, split on or off
void RIG_FT710::set_split(bool split_on) {
	split = split_on;
	if (split_on) {
		cmd = "ST1;";
		sendCommand(cmd);
		sett("Split ON");
	} else {
		cmd = "ST0;";
		sendCommand(cmd);
		sett("Split OFF");
	}
}

/// FT, split is on when the transmitter is not on the main VFO
int RIG_FT710::get_split() {
	cmd = rsp = "FT";
	cmd += ";";
	wait_char(';', 4, 100, "Get split", ASC);
	gett("get split()");
	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return 0;
	}
	int tx_vfo = replystr[pos + 2] - '0';

	return (tx_vfo > 0);
}

/// SV, swap VFO A and VFO B
void RIG_FT710::swapAB() {
	cmd = "SV;";
	sendCommand(cmd);
	sett("swapAB()");
}

/// S meter, SM0 (manual p.20): answer SM0 P2P2P2 ; is 7 characters.
/// Returns -1, which leaves the S meter unchanged, when the reply is
/// missing or is the reply to another command.
int RIG_FT710::get_smeter() {
	cmd = rsp = "SM0";
	cmd += ';';
	wait_char(';', 7, 100, "get smeter", ASC);

	gett("get_smeter()");

	size_t pos = last_frame("SM0", 7);
	if (pos == std::string::npos) {
		return -1;
	}

	int meter = atoi(replystr.substr(pos + 3, 3).c_str());
	meter = meter * 100.0 / 256.0;
	return meter;
}

/// Convert a raw RM reading with a table of {raw, value} points.
/// Readings below the first point or above the last one take the end
/// value, so the lookup never runs off the table.
static double meter_value(const meterpair *table, size_t points, int raw) {
	if (raw <= table[0].mtr) {
		return table[0].val;
	}
	for (size_t point = 1; point < points; point++) {
		if (raw <= table[point].mtr) {
			const meterpair &low = table[point - 1];
			const meterpair &high = table[point];
			return low.val + (high.val - low.val) *
				(raw - low.mtr) / (high.mtr - low.mtr);
		}
	}
	return table[points - 1].val;
}

/// RM6 to flrig's SWR bar.  SWR points from Hamlib rigs/yaesu/ft710.h
/// FT710_SWR_CAL (LGPL-2.1-or-later), measured by G3VPX on an FTDX101D:
/// raw 26 = 1.2, 52 = 1.5, 89 = 2.0, 126 = 3.0, 173 = 4.0, 236 = 5.0.
/// Each SWR is placed on flrig's bar (1.5 at 10, 2.0 at 23, 3.0 at 48,
/// 10 at 100), the scale used by rig.get_SWR in xml_server.cxx.
static const meterpair SWR_TABLE[] = {
	{   0,   0 },
	{  26,   4 },
	{  52,  10 },
	{  89,  23 },
	{ 126,  48 },
	{ 173,  55 },
	{ 236,  63 },
	{ 255, 100 }
};

/// RM5 to watts, measured on one FT-710 (N0RC, 2026-10-01): steady RM5
/// in RTTY at each PC setting.  The reference is the PC setting, not a
/// wattmeter.
static const meterpair POWER_TABLE[] = {
	{   0,   0 },
	{  47,   5 },
	{  65,  10 },
	{ 100,  25 },
	{ 140,  50 },
	{ 169,  75 },
	{ 185, 100 }
};

/// RM7 to amps: 0-255 = 0-25.5 A, from Hamlib rigs/yaesu/ft991.h
/// FT991_ID_CAL (LGPL-2.1-or-later, corrected in Hamlib issue #2073).
/// Hamlib's FT710_ID_CAL stops at 10 A, below the FT-710's 100 W draw.
/// An FT-710 read 55 at 5 W and 167 at 100 W, i.e. 5.5 A and 16.7 A.
static const meterpair IDD_TABLE[] = {
	{   0,  0.0 },
	{ 255, 25.5 }
};

/// RM8 to volts, measured on one FT-710 (N0RC, 2026-10-01): RM8 214
/// in receive with 13.97 V measured at the radio, as a line through 0.
/// Hamlib's FT710_VD_CAL (192 = 13.8 V, marked TBC) is from the FT-991
/// and reads 15.6 V for this radio.
static const meterpair VDD_TABLE[] = {
	{   0,  0.00 },
	{ 214, 13.97 },
	{ 255, 16.65 }
};

/// Read meter RM P1 (manual p.19): answer RM P1 P2P2P2 000 ; is 10
/// characters.  Sent once; the radio answers every RM it receives, and
/// a second answer is read by the next command.  Returns the raw value
/// 0-255, or the last good value when the reply is missing or is the
/// reply to another command.
int RIG_FT710::read_meter(int meter, const char *label) {
	char prefix[4];
	snprintf(prefix, sizeof(prefix), "RM%d", meter);

	cmd = rsp = prefix;
	cmd += ';';
	wait_char(';', 10, 100, label, ASC);
	gett(label);

	size_t pos = last_frame(prefix, 10);
	if (pos == std::string::npos) {
		return m_meter_raw[meter];
	}
	for (size_t digit = pos + 3; digit < pos + 6; digit++) {
		if (!isdigit(replystr[digit])) {
			return m_meter_raw[meter];
		}
	}

	m_meter_raw[meter] = atoi(replystr.substr(pos + 3, 3).c_str());
	return m_meter_raw[meter];
}

/// RM6, SWR bar 0 - 100
int RIG_FT710::get_swr() {
	int raw = read_meter(6, "get swr");

	int bar = (int)round(meter_value(SWR_TABLE,
		sizeof(SWR_TABLE) / sizeof(meterpair), raw));
	LOG_DEBUG("RM6 %d = SWR bar %d", raw, bar);
	return bar;
}

/// RM7, drain current in amps
double RIG_FT710::get_idd() {
	int raw = read_meter(7, "get idd");

	double amps = meter_value(IDD_TABLE,
		sizeof(IDD_TABLE) / sizeof(meterpair), raw);
	LOG_DEBUG("RM7 %d = %.1f A", raw, amps);
	return amps;
}

/// RM8, supply voltage in volts
double RIG_FT710::get_voltmeter() {
	int raw = read_meter(8, "get vdd");

	double volts = meter_value(VDD_TABLE,
		sizeof(VDD_TABLE) / sizeof(meterpair), raw);
	LOG_DEBUG("RM8 %d = %.2f V", raw, volts);
	return volts;
}

/// RM5, power out in watts
int RIG_FT710::get_power_out() {
	int raw = read_meter(5, "get pout");

	int watts = (int)round(meter_value(POWER_TABLE,
		sizeof(POWER_TABLE) / sizeof(meterpair), raw));
	LOG_DEBUG("RM5 %d = %d W", raw, watts);
	return watts;
}

/// RM4, ALC meter 0 - 100
int RIG_FT710::get_alc() {
	int raw = read_meter(4, "get alc");

	int alc = (int)ceil(raw / 2.56);
	LOG_DEBUG("RM4 %d = ALC %d", raw, alc);
	return alc;
}

/// PC, transceiver power level in watts
double RIG_FT710::get_power_control() {
	cmd = rsp = "PC";
	cmd += ';';
	wait_char(';', 6, 100, "get power", ASC);

	gett("get_power_control()");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return progStatus.power_level;
	}
	if (pos + 5 >= replystr.length()) {
		return progStatus.power_level;
	}

	int power = atoi(&replystr[pos + 2]);
	return power;
}

/// PC, transceiver power level in watts
void RIG_FT710::set_power_control(double watts) {
	int power = (int)watts;
	cmd = "PC000;";
	for (int i = 4; i > 1; i--) {
		cmd[i] += power % 10;
		power /= 10;
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "SET power", cmd, replystr);
}

/// AG0, volume control 0 - 100
int RIG_FT710::get_volume_control() {
	cmd = rsp = "AG0";
	cmd += ';';
	wait_char(';', 7, 100, "get vol", ASC);

	gett("get_volume_control()");

	size_t pos = last_frame("AG0", 7);
	if (pos == std::string::npos) {
		return progStatus.volume;
	}
	int volume = 0;
	sscanf(&replystr[pos], "AG0%d", &volume);
	volume = (int)round(volume * 100 / 255.0);
	if (volume > 100) {
		volume = 100;
	}
	return volume;
}

/// AG0, volume control 0 - 100; AF GAIN is 000-255 (manual p.6)
void RIG_FT710::set_volume_control(int volume) {
	int ivol = (int)round(volume * 255 / 100.0);
	cmd = "AG0000;";
	for (int i = 5; i > 2; i--) {
		cmd[i] += ivol % 10;
		ivol /= 10;
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "SET vol", cmd, replystr);
}

/// TX, transceiver PTT on/off
void RIG_FT710::set_PTT_control(int ptt_on) {
	cmd = ptt_on ? "TX1;" : "TX0;";
	sendCommand(cmd);
	showresp(WARN, ASC, "SET PTT", cmd, replystr);
	ptt_ = ptt_on;
}

/// TX, transceiver PTT state
int RIG_FT710::get_PTT() {
	cmd = "TX;";
	rsp = "TX";
	wait_char(';', 4, 100, "get PTT", ASC);

	gett("get_PTT()");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return ptt_;
	}
	ptt_ = (replystr[pos + 2] != '0' ? 1 : 0);
	return ptt_;
}

/// AC, tuner off (0), on (1) or start tuning (2)
void RIG_FT710::tune_rig(int action) {
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
	sendCommand(cmd);
	showresp(WARN, ASC, "tune rig", cmd, replystr);
	sett("tune_rig");
}

/// AC, 1 if the tuner is on
int RIG_FT710::get_tune() {
	cmd = rsp = "AC";
	cmd += ';';
	wait_char(';', 6, 100, "get tune", ASC);

	rig_trace(2, "get_tuner status()", replystr.c_str());

	size_t pos = last_frame("AC", 6);
	if (pos == std::string::npos) {
		return 0;
	}
	if (replystr[pos + 4] == '0') {
		return 0;
	}
	return 1;
}

/// attenuator cycles off, 6 dB, 12 dB, 18 dB
int RIG_FT710::next_attenuator() {
	switch (atten_state) {
		case 0:
			return 1;
		case 1:
			return 2;
		case 2:
			return 3;
		case 3:
			return 0;
	}
	return 0;
}

/// RA0, attenuator 0 - 3
void RIG_FT710::set_attenuator(int atten) {
	atten_state = atten;
	cmd = "RA00;";
	cmd[3] += atten_state;
	sendCommand(cmd);
	showresp(WARN, ASC, "SET att", cmd, replystr);
}

/// RA0, attenuator 0 - 3
int RIG_FT710::get_attenuator() {
	cmd = rsp = "RA0";
	cmd += ';';
	wait_char(';', 5, 100, "get att", ASC);

	gett("get_attenuator()");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return progStatus.attenuator;
	}
	if (pos + 3 >= replystr.length()) {
		return progStatus.attenuator;
	}
	atten_state = replystr[pos + 3] - '0';
	return atten_state;
}

/// preamp cycles IPO, AMP 1, AMP 2
int RIG_FT710::next_preamp() {
	switch (preamp_state) {
		case 0:
			return 1;
		case 1:
			return 2;
		case 2:
			return 0;
	}
	return 0;
}

/// PA0, preamp 0 - 2
void RIG_FT710::set_preamp(int preamp) {
	preamp_state = preamp;
	cmd = "PA00;";
	cmd[3] = '0' + preamp_state;
	sendCommand(cmd);
	showresp(WARN, ASC, "SET preamp", cmd, replystr);
}

/// PA0, preamp 0 - 2
int RIG_FT710::get_preamp() {
	cmd = rsp = "PA0";
	cmd += ';';
	wait_char(';', 5, 100, "get pre", ASC);

	gett("get_preamp()");

	size_t pos = replystr.rfind(rsp);
	if (pos != std::string::npos) {
		preamp_state = replystr[pos + 3] - '0';
	}
	return preamp_state;
}

// default bandwidth table: 0 - wide, 1 - narrow
static bool narrow = 0;

/// AM, FM and DATA-FM have no width setting; AM-N, FM-N and DATA-FM-N
/// have one fixed width (manual Table 3, p.20)
bool RIG_FT710::fixed_width(int mode) {
	return mode == mFM || mode == mAM || mode == mFM_N ||
		mode == mDATA_FM || mode == mAM_N || mode == mDATA_FMN;
}

/// select the width table for mode and return its default width index
int RIG_FT710::adjust_bandwidth(int mode) {
	int bw_index = 0;
	if (mode == mCW_U || mode == mCW_L) {
		bandwidths_ = ft710_widths_cw;
		bw_vals_ = FT710_WVALS_CW;
	} else if (fixed_width(mode)) {
		if (mode == mFM) {
			bandwidths_ = ft710_widths_fm_wide;
		} else if (mode == mAM) {
			bandwidths_ = ft710_widths_am_wide;
		} else if (mode == mAM_N) {
			bandwidths_ = ft710_widths_am_nar;
		} else if (mode == mFM_N) {
			bandwidths_ = ft710_widths_fm_nar;
		} else if (mode == mDATA_FM) {
			bandwidths_ = ft710_widths_data_fm;
		} else if (mode == mDATA_FMN) {
			bandwidths_ = ft710_widths_data_fmn;
		}
		bw_vals_ = FT710_WVALS_AMFM;
	} else if (mode == mRTTY_L || mode == mRTTY_U) {
		bandwidths_ = ft710_widths_rtty;
		bw_vals_ = FT710_WVALS_RTTY;
	} else if (mode == mDATA_L || mode == mDATA_U || mode == mPSK) {
		bandwidths_ = ft710_widths_data;
		bw_vals_ = FT710_WVALS_PSK;
	} else {
		bandwidths_ = ft710_widths_ssb;
		bw_vals_ = FT710_WVALS_SSB;
	}

	if (narrow) {
		bw_index = FT710_DEF_BW_NARROW[mode];
	} else {
		bw_index = FT710_DEF_BW_WIDE[mode];
	}

	return bw_index;
}

/// width index last used in mode on the VFO in use, else the default
int RIG_FT710::def_bandwidth(int mode) {
	int bw_index = adjust_bandwidth(mode);
	if (inuse == onB) {
		if (mode_bwB[mode] == -1) {
			mode_bwB[mode] = bw_index;
		}
		return mode_bwB[mode];
	}
	if (mode_bwA[mode] == -1) {
		mode_bwA[mode] = bw_index;
	}
	return mode_bwA[mode];
}

/// width table for mode
std::vector<std::string>& RIG_FT710::bwtable(int mode) {
	switch (mode) {
		case mCW_U:
		case mCW_L:
			return ft710_widths_cw;
		case mFM:
			return ft710_widths_fm_wide;
		case mAM:
			return ft710_widths_am_wide;
		case mAM_N:
			return ft710_widths_am_nar;
		case mRTTY_L:
		case mRTTY_U:
			return ft710_widths_rtty;
		case mDATA_L:
		case mDATA_U:
		case mPSK:
			return ft710_widths_data;
		case mFM_N:
			return ft710_widths_fm_nar;
		case mDATA_FMN:
			return ft710_widths_data_fmn;
		case mDATA_FM:
			return ft710_widths_data_fm;
		default:
			break;
	}
	return ft710_widths_ssb;
}

/// MD, mode of VFO A
void RIG_FT710::set_modeA(int mode) {
	modeA = mode;
	if (inuse == onB) {
		cmd = rsp = "MD1";
	} else {
		cmd = rsp = "MD0";
	}
	cmd += FT710_MODE_CHR[mode];
	cmd += ';';
	sendCommand(cmd);
	showresp(WARN, ASC, "SET mode A", cmd, replystr);
	adjust_bandwidth(modeA);
}

/// MD, mode of VFO A
int RIG_FT710::get_modeA() {
	if (inuse == onB) {
		cmd = rsp = "MD1";
	} else {
		cmd = rsp = "MD0";
	}
	cmd += ';';
	wait_char(';', 5, 100, "get mode A", ASC);

	gett("get_modeA()");

	size_t pos = replystr.rfind(rsp);
	if (pos != std::string::npos) {
		if (pos + 3 < replystr.length()) {
			int mode_chr = replystr[pos + 3];
			int mode = 0;
			for (mode = 0; mode < NUM_MODES; mode++) {
				if (mode_chr == FT710_MODE_CHR[mode]) {
					break;
				}
			}
			if (mode < NUM_MODES) {
				modeA = mode;
			}
		}
	}
	adjust_bandwidth(modeA);
	return modeA;
}

/// MD, mode of VFO B
void RIG_FT710::set_modeB(int mode) {
	modeB = mode;
	if (inuse == onA) {
		cmd = rsp = "MD1";
	} else {
		cmd = rsp = "MD0";
	}
	cmd += FT710_MODE_CHR[mode];
	cmd += ';';
	sendCommand(cmd);
	showresp(WARN, ASC, "SET mode B", cmd, replystr);
	adjust_bandwidth(modeB);
}

/// MD, mode of VFO B
int RIG_FT710::get_modeB() {
	if (inuse == onA) {
		cmd = rsp = "MD1";
	} else {
		cmd = rsp = "MD0";
	}
	cmd += ';';
	wait_char(';', 5, 100, "get mode B", ASC);

	gett("get_modeB()");

	size_t pos = replystr.rfind(rsp);
	if (pos != std::string::npos) {
		if (pos + 3 < replystr.length()) {
			int mode_chr = replystr[pos + 3];
			int mode = 0;
			for (mode = 0; mode < NUM_MODES; mode++) {
				if (mode_chr == FT710_MODE_CHR[mode]) {
					break;
				}
			}
			if (mode < NUM_MODES) {
				modeB = mode;
			}
		}
	}
	adjust_bandwidth(modeB);
	return modeB;
}

/// SH0, width of VFO A
void RIG_FT710::set_bwA(int bw_index) {
	int bw_code = bw_vals_[bw_index];
	bwA = bw_index;

	if (fixed_width(modeA)) {
		return;
	}
	cmd.clear();
	cmd.append("SH00");
	cmd += '0' + bw_code / 10;
	cmd += '0' + bw_code % 10;
	cmd += ';';
	sendCommand(cmd);
	showresp(WARN, ASC, "SET bw A", cmd, replystr);
	sett("SET bwA");
	mode_bwA[modeA] = bw_index;
}

/// SH0, width of VFO A
int RIG_FT710::get_bwA() {
	if (fixed_width(modeA)) {
		bwA = 0;
		mode_bwA[modeA] = bwA;
		return bwA;
	}
	cmd = rsp = "SH0";
	cmd += ';';
	wait_char(';', 7, 100, "get bw A", ASC);

	gett("get_bwA()");

	size_t pos = last_frame("SH0", 7);
	if (pos == std::string::npos) {
		return bwA;
	}

	replystr[pos + 6] = 0;
	int bw_code = fm_decimal(replystr.substr(pos + 4), 2);

	const int *wval = bw_vals_;
	int bw_index = 0;
	while (*wval != WVALS_LIMIT) {
		if (*wval == bw_code) {
			break;
		}
		wval++;
		bw_index++;
	}
	if (*wval == WVALS_LIMIT) {
		bw_index = 0;
	}
	bwA = bw_index;
	mode_bwA[modeA] = bwA;
	return bwA;
}

/// SH0, width of VFO B
void RIG_FT710::set_bwB(int bw_index) {
	int bw_code = bw_vals_[bw_index];
	bwB = bw_index;

	if (fixed_width(modeB)) {
		mode_bwB[modeB] = 0;
		return;
	}
	cmd.clear();
	cmd.append("SH00");
	cmd += '0' + bw_code / 10;
	cmd += '0' + bw_code % 10;
	cmd += ';';
	sendCommand(cmd);
	showresp(WARN, ASC, "SET bw B", cmd, replystr);
	sett("SET bwB");
	mode_bwB[modeB] = bwB;
}

/// SH0, width of VFO B
int RIG_FT710::get_bwB() {
	if (fixed_width(modeB)) {
		bwB = 0;
		mode_bwB[modeB] = bwB;
		return bwB;
	}
	cmd = rsp = "SH0";
	cmd += ';';
	wait_char(';', 7, 100, "get bw B", ASC);

	gett("get_bwB()");

	size_t pos = last_frame("SH0", 7);
	if (pos == std::string::npos) {
		return bwB;
	}

	replystr[pos + 6] = 0;
	int bw_code = fm_decimal(replystr.substr(pos + 4), 2);

	const int *wval = bw_vals_;
	int bw_index = 0;
	while (*wval != WVALS_LIMIT) {
		if (*wval == bw_code) {
			break;
		}
		wval++;
		bw_index++;
	}
	if (*wval == WVALS_LIMIT) {
		bw_index = 0;
	}
	bwB = bw_index;
	mode_bwB[modeB] = bwB;
	return bwB;
}

/// width index last used in each mode, VFO A then VFO B
std::string RIG_FT710::get_BANDWIDTHS() {
	std::stringstream widths;
	for (int i = 0; i < NUM_MODES; i++) {
		widths << mode_bwA[i] << " ";
	}
	for (int i = 0; i < NUM_MODES; i++) {
		widths << mode_bwB[i] << " ";
	}
	return widths.str();
}

/// width index last used in each mode, VFO A then VFO B
void RIG_FT710::set_BANDWIDTHS(std::string widths) {
	std::stringstream stream;
	stream << widths;
	for (int i = 0; i < NUM_MODES; i++) {
		stream >> mode_bwA[i];
	}
	for (int i = 0; i < NUM_MODES; i++) {
		stream >> mode_bwB[i];
	}
}

/// 'L' for lower sideband modes, otherwise 'U'
int RIG_FT710::get_modetype(int mode) {
	return FT710_MODE_TYPE[mode];
}

/// IS P1 is fixed at 0 (manual p.14); there is one IF shift for A and B
void RIG_FT710::set_if_shift(int shift) {
	cmd = "IS00+0000;";
	if (shift != 0) {
		progStatus.shift = true;
	} else {
		progStatus.shift = false;
	}
	if (shift < 0) {
		cmd[4] = '-';
	}
	shift = abs(shift);
	for (int i = 8; i > 4; i--) {
		cmd[i] += shift % 10;
		shift /= 10;
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "SET if shift", cmd, replystr);
}

/// IS0, IF shift in Hz; true if not 0
bool RIG_FT710::get_if_shift(int &shift) {
	cmd = rsp = "IS0";
	cmd += ';';
	wait_char(';', 10, 100, "get if shift", ASC);

	gett("get_if_shift()");

	size_t pos = replystr.rfind(rsp);
	shift = progStatus.shift_val;
	if (pos == std::string::npos) {
		return progStatus.shift;
	}
	shift = atoi(&replystr[pos + 5]);
	if (replystr[pos + 4] == '-') {
		shift = -shift;
	}
	return (shift != 0);
}

/// IF shift range -1200 to 1200 Hz in 20 Hz steps
void RIG_FT710::get_if_min_max_step(int &min, int &max, int &step) {
	if_shift_min = min = -1200;
	if_shift_max = max = 1200;
	if_shift_step = step = 20;
	if_shift_mid = 0;
}

// BP P1 P2 P3P3P3 ;
//   P1: '0' manual notch on/off, '1' notch frequency
//   P2: fixed '0'
//   P3: on/off 000/001, or frequency 001 - 320 (x 10 Hz)
static const std::string NOTCH_STR_ON  = "BP00001;";
static const std::string NOTCH_STR_OFF = "BP00000;";
static std::string notch_str_val = "BP01000;";
static int notch_val = 1500;

/// BP, manual notch on/off and frequency in Hz
void RIG_FT710::set_notch(bool notch_on, int frequency) {
	if (notch_val != frequency) {
// set notch ON
		cmd = NOTCH_STR_ON;
		sendCommand(cmd);
		showresp(WARN, ASC, "SET notch ON", cmd, replystr);
		set_trace(3, "set_notch ON", cmd.c_str(), replystr.c_str());
// set notch frequency
		notch_val = frequency;
		frequency /= 10;
		for (int i = 0; i < 3; i++) {
			notch_str_val[6 - i] = '0' + (frequency % 10);
			frequency /= 10;
		}
		cmd = notch_str_val;
		sendCommand(cmd);
		showresp(WARN, ASC, "SET notch val", cmd, replystr);
		set_trace(3, "set_notch val", cmd.c_str(), replystr.c_str());
	}
	if (notch_on) {
		cmd = NOTCH_STR_ON;
	} else {
		cmd = NOTCH_STR_OFF;
	}
	sendCommand(cmd);
	set_trace(3, "set_notch OFF", cmd.c_str(), replystr.c_str());
	showresp(WARN, ASC, "SET notch OFF", cmd, replystr);
}

/// BP00 and BP01, manual notch on/off and frequency in Hz
bool RIG_FT710::get_notch(int &frequency) {
	bool notch_is_on = false;

	cmd = "BP00;";
	rsp = "BP";
	wait_char(';', 8, 100, "get notch on/off", ASC);
	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return notch_is_on;
	}

	gett("get_notch()");

// manual notch enabled
	if (replystr[pos + 6] == '1') {
		notch_is_on = true;
	}

	frequency = progStatus.notch_val;
	cmd = "BP01;";
	rsp = "BP";
	wait_char(';', 8, 100, "get notch val", ASC);

	gett("get_notch_val()");

	pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		frequency = 10;
	} else {
		frequency = fm_decimal(replystr.substr(pos + 4), 3) * 10;
	}

	return (m_notch_on = notch_is_on);
}

/// manual notch range 10 to 3200 Hz in 10 Hz steps
void RIG_FT710::get_notch_min_max_step(int &min, int &max, int &step) {
	min = 10;
	max = 3200;
	step = 10;
}

/// BC P1 is fixed at 0 (manual p.7); the radio answers ?; to BC1
void RIG_FT710::set_auto_notch(int notch_on) {
	cmd = "BC00;";
	if (notch_on) {
		cmd[3] = '1';
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "SET auto notch", cmd, replystr);
}

/// BC0, auto notch on/off
int RIG_FT710::get_auto_notch() {
	cmd = "BC0;";
	wait_char(';', 5, 100, "get auto notch", ASC);

	gett("get_auto_notch()");

	size_t pos = replystr.rfind("BC");
	if (pos == std::string::npos) {
		return 0;
	}
	if (replystr[pos + 3] == '1') {
		return 1;
	}
	return 0;
}

/// NB P1 is fixed at 0 (manual p.17); there is one NB for A and B
void RIG_FT710::set_noise(bool nb_on) {
	cmd = "NB00;";

	nb_state = nb_on;

	if (nb_on) {
		cmd[3] = '1';
		noise_blanker_label(nb_label(), true);
	} else {
		noise_blanker_label(nb_label(), false);
	}

	sendCommand(cmd);
	showresp(WARN, ASC, "SET NB", cmd, replystr);
}

/// NB0, noise blanker on/off
int RIG_FT710::get_noise() {
	cmd = rsp = "NB0";
	cmd += ';';
	wait_char(';', 5, 100, "get NB", ASC);

	gett("get_noise()");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return nb_state;
	}

	nb_state = replystr[pos + 3] - '0';

	if (nb_state) {
		noise_blanker_label("NB on", true);
	} else {
		noise_blanker_label("NB", false);
	}

	return nb_state;
}

/// MG, mic gain 0 - 100
void RIG_FT710::set_mic_gain(int gain) {
	cmd = "MG000;";
	for (int i = 3; i > 0; i--) {
		cmd[1 + i] += gain % 10;
		gain /= 10;
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "SET mic", cmd, replystr);
}

/// MG, mic gain 0 - 100
int RIG_FT710::get_mic_gain() {
	cmd = rsp = "MG";
	cmd += ';';
	wait_char(';', 6, 100, "get mic", ASC);

	gett("get_mic_gain()");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return progStatus.mic_gain;
	}
	int gain = atoi(&replystr[pos + 2]);
	return gain;
}

/// mic gain range 0 - 100
void RIG_FT710::get_mic_min_max_step(int &min, int &max, int &step) {
	min = 0;
	max = 100;
	step = 1;
}

/// RG0, RF gain 0 - 100; RF GAIN is 000-255 (manual p.19)
void RIG_FT710::set_rf_gain(int gain) {
	cmd = "RG0000;";
	int rfval = (int)round(gain * 255 / 100.0);
	for (int i = 5; i > 2; i--) {
		cmd[i] = rfval % 10 + '0';
		rfval /= 10;
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "SET rfgain", cmd, replystr);

/// An FT-710 now and then answers ?; to a valid RG set during flrig's
/// polling (2 of 99 sets in testing) and keeps the old value, so read
/// the setting back and send it once more if it differs.
	std::string sent = cmd;
	cmd = "RG0;";
	wait_char(';', 7, 100, "check rfgain", ASC);
	size_t pos = last_frame("RG0", 7);
	if (pos == std::string::npos || replystr.substr(pos, 7) != sent) {
		LOG_WARN("%s not taken, read back \"%s\", sending again",
			sent.c_str(), replystr.c_str());
		cmd = sent;
		sendCommand(cmd);
		showresp(WARN, ASC, "SET rfgain again", cmd, replystr);
	}
}

/// RG0, RF gain 0 - 100
int RIG_FT710::get_rf_gain() {
	int rfval = 0;
	cmd = rsp = "RG0";
	cmd += ';';
	wait_char(';', 7, 100, "get rfgain", ASC);

	gett("get_rf_gain()");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return progStatus.rfgain;
	}
	for (int i = 3; i < 6; i++) {
		rfval *= 10;
		rfval += replystr[pos + i] - '0';
	}
	rfval = (int)round(rfval * 100 / 255.0);
	if (rfval > 100) {
		rfval = 100;
	}
	return rfval;
}

/// RF gain range 0 - 100
void RIG_FT710::get_rf_min_max_step(int &min, int &max, int &step) {
	min = 0;
	max = 100;
	step = 1;
}

/// VX, VOX on/off
void RIG_FT710::set_vox_onoff() {
	cmd = "VX0;";
	if (progStatus.vox_onoff) {
		cmd[2] = '1';
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "SET vox", cmd, replystr);
}

/// VG, VOX gain
void RIG_FT710::set_vox_gain() {
	cmd = "VG";
	cmd.append(to_decimal(progStatus.vox_gain, 3)).append(";");
	sendCommand(cmd);
	showresp(WARN, ASC, "SET vox gain", cmd, replystr);
}

/// ANTI VOX LEVEL, AV 001-100 (manual p.7)
void RIG_FT710::set_vox_anti() {
	int level = progStatus.vox_anti;
	if (level < 1) {
		level = 1;
	}
	if (level > 100) {
		level = 100;
	}
	cmd.assign("AV").append(to_decimal(level, 3)).append(";");
	sendCommand(cmd);
	showresp(WARN, ASC, "SET anti-vox", cmd, replystr);
}

/// VD, VOX delay
void RIG_FT710::set_vox_hang() {
	cmd = "VD";
	cmd.append(to_decimal(progStatus.vox_hang, 4)).append(";");
	sendCommand(cmd);
	showresp(WARN, ASC, "SET vox delay", cmd, replystr);
}

/// EX030405, VOX on the data port
void RIG_FT710::set_vox_on_dataport() {
	cmd = "EX0304050;";
	if (progStatus.vox_on_dataport) {
		cmd[8] = '1';
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "SET vox on data port", cmd, replystr);
}

/// KS, keyer speed 4 - 60 wpm
void RIG_FT710::set_cw_wpm() {
	cmd = "KS";
	if (progStatus.cw_wpm > 60) {
		progStatus.cw_wpm = 60;
	}
	if (progStatus.cw_wpm < 4) {
		progStatus.cw_wpm = 4;
	}
	cmd.append(to_decimal(progStatus.cw_wpm, 3)).append(";");
	sendCommand(cmd);
	showresp(WARN, ASC, "SET cw wpm", cmd, replystr);
}

/// KR, keyer on/off
void RIG_FT710::enable_keyer() {
	cmd = "KR0;";
	if (progStatus.enable_keyer) {
		cmd[2] = '1';
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "SET keyer on/off", cmd, replystr);
}

/// CS, CW spot on/off; only sent in CW-U or CW-L
bool RIG_FT710::set_cw_spot() {
	if (vfo->imode == 2 || vfo->imode == 6) {
		cmd = "CS0;";
		if (progStatus.spot_onoff) {
			cmd[2] = '1';
		}
		sendCommand(cmd);
		showresp(WARN, ASC, "SET spot on/off", cmd, replystr);
		return true;
	} else {
		return false;
	}
}

/// CW WEIGHT is EX020203 (manual p.11).  The manual gives P4 as 25-45,
/// but an FT-710 answers ?; to 25 and takes 00-20 for 2.5-4.5.
void RIG_FT710::set_cw_weight() {
	int weight = round((progStatus.cw_weight - 2.5) * 10);
	if (weight < 0) {
		weight = 0;
	}
	if (weight > 20) {
		weight = 20;
	}
	cmd.assign("EX020203").append(to_decimal(weight, 2)).append(";");
	sendCommand(cmd);
	showresp(WARN, ASC, "SET cw weight", cmd, replystr);
}

/// QSK DELAY TIME is EX020119 (manual p.11): 0-3 for 15-30 msec
void RIG_FT710::set_cw_qsk() {
	int delay = progStatus.cw_qsk / 5 - 3;
	if (delay < 0) {
		delay = 0;
	}
	if (delay > 3) {
		delay = 3;
	}
	cmd.assign("EX020119").append(to_decimal(delay, 1)).append(";");
	sendCommand(cmd);
	showresp(WARN, ASC, "SET cw qsk", cmd, replystr);
}

/// BI, break-in on/off
void RIG_FT710::set_break_in() {
	if (progStatus.break_in) {
		cmd = "BI1;";
		break_in_label("BK-IN");
	} else {
		cmd = "BI0;";
		break_in_label("QSK ?");
	}
	sendCommand(cmd);
	showresp(WARN, ASC, "SET break in on/off", cmd, replystr);
	sett("set_break_in");
}

/// BI, break-in on/off.  The FT-710 answers BI only in CW; in other
/// modes it answers ?;
int RIG_FT710::get_break_in() {
	int mode = (inuse == onB) ? modeB : modeA;
	if (mode != mCW_U && mode != mCW_L) {
		return progStatus.break_in;
	}
	cmd = "BI;";
	wait_char(';', 4, 100, "get break in", ASC);
	size_t pos = last_frame("BI", 4);
	if (pos == std::string::npos) {
		return progStatus.break_in;
	}
	progStatus.break_in = (replystr[pos + 2] == '1');
	if (progStatus.break_in) {
		break_in_label("BK-IN");
		progStatus.cw_delay = 0;
	} else {
		break_in_label("QSK ?");
	}
	return progStatus.break_in;
}

/// RL0, DNR level
void RIG_FT710::set_noise_reduction_val(int level) {
	cmd.assign("RL0").append(to_decimal(level, 2)).append(";");
	sendCommand(cmd);
	showresp(WARN, ASC, "SET_noise_reduction_val", cmd, replystr);
	sett("set_noise_reduction_val");
}

/// RL0, DNR level
int RIG_FT710::get_noise_reduction_val() {
	int level = 1;
	cmd = rsp = "RL0";
	cmd.append(";");
	wait_char(';', 6, 100, "GET noise reduction val", ASC);
	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return level;
	}
	level = atoi(&replystr[pos + 3]);
	return level;
}

/// NR0, DNR on/off
void RIG_FT710::set_noise_reduction(int nr_on) {
	cmd.assign("NR0").append(nr_on ? "1" : "0").append(";");
	sendCommand(cmd);
	showresp(WARN, ASC, "SET noise reduction", cmd, replystr);
	sett("set_noise_reduction_on/off");
}

/// NR0, DNR on/off
int RIG_FT710::get_noise_reduction() {
	cmd = rsp = "NR0";
	cmd.append(";");
	wait_char(';', 5, 100, "GET noise reduction", ASC);
	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return 0;
	}
	int nr_on = replystr[pos + 3] - '0';
	return nr_on;
}

/// DT0, set the date; date_str is formatted as YYYYMMDD
void RIG_FT710::sync_date(char *date_str) {
	cmd.assign("DT0");
	cmd.append(date_str);
	cmd += ';';
	sendCommand(cmd);
	showresp(WARN, ASC, "sync_date", cmd, replystr);
	sett("sync_date");
}

/// DT1, set the time; time_str is formatted as HH:MM:SS
void RIG_FT710::sync_clock(char *time_str) {
	cmd.assign("DT1");
	cmd += time_str[0];
	cmd += time_str[1];
	cmd += time_str[3];
	cmd += time_str[4];
	cmd += time_str[6];
	cmd += time_str[7];
	cmd += ';';
	sendCommand(cmd);
	showresp(WARN, ASC, "sync_time", cmd, replystr);
	sett("sync_time");
}

/// SQ0, squelch 0 - 100
void RIG_FT710::set_squelch(int level) {
	cmd = "SQ0000;";
	for (int i = 5; i > 2; i--) {
		cmd[i] = level % 10 + '0';
		level /= 10;
	}

	set_trace(1, "set_squelch()");
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, "SET squelch", cmd, replystr);
}

/// SQ0, squelch 0 - 100
int RIG_FT710::get_squelch() {
	int sqval = 0;
	cmd = rsp = "SQ0";
	cmd += ';';
	get_trace(1, "get_squelch()");
	wait_char(';', 7, 100, "get squelch", ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos) {
		return progStatus.squelch;
	}
	for (int i = 3; i < 6; i++) {
		sqval *= 10;
		sqval += replystr[pos + i] - '0';
	}
	return ceil(sqval);
}
