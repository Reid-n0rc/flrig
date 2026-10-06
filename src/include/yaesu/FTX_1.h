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

#ifndef YAESU_FTX_1_H_INCLUDED
#define YAESU_FTX_1_H_INCLUDED

#include "rigbase.h"

/// Yaesu FTX-1, CAT Operation Reference Manual 2508-C
class RIG_FTX_1 : public rigbase {
private:
/// PC P1 of the fitted head: '1' field head, '2' SPA-1, '0' not known
	char m_head;

/// noise blanker / noise reduction state last sent or read
	bool m_nb_on;
	int  m_nb_level;
	bool m_nr_on;
	int  m_nr_level;

	char active_side();
	bool fixed_width(int mode);
	void set_width(char side, int bw_code);
	int  read_width(const char *answer);

	void read_head();
	int  get_meter(char meter);

	void set_dsp_level(const char *command, int digits, int level);
	int  get_dsp_level(const char *command, int digits);
	void read_dsp(const char *command, int digits, bool &is_on, int &level);

	char preamp_band();
	int  preamp_max(char band);
	void preamp_labels(char band);

protected:
	int  m_atten_level;

public:
	RIG_FTX_1();
	~RIG_FTX_1() {}

	void initialize();
	void post_initialize();

	bool check();

	unsigned long long get_vfoA();
	void set_vfoA(unsigned long long frequency);

	unsigned long long get_vfoB();
	void set_vfoB(unsigned long long frequency);

	bool twovfos() {
		return true;
	}
	bool canswap() {
		return true;
	}

	int  get_vfoAorB();
	void selectA();
	void selectB();

	void A2B();
	void B2A();
	void swapAB();

	bool can_split();
	void set_split(bool split_on);
	int  get_split();

	void set_modeA(int mode);
	int  get_modeA();
	int  get_modetype(int mode);

	void set_modeB(int mode);
	int  get_modeB();

	void set_bwA(int bw_index);
	int  get_bwA();

	void set_bwB(int bw_index);
	int  get_bwB();

	const char *get_bwname_(int bw_index, int mode);

	int  adjust_bandwidth(int mode);
	int  def_bandwidth(int mode);

	int  get_agc();
	int  incr_agc();
	const char *agc_label();
	int  agc_val();

	int  get_smeter();
	int  get_swr();
	int  get_alc();
	int  get_power_out();

	double get_power_control();
	void   set_power_control(double watts);
	void   get_pc_min_max_step(double &min, double &max, double &step);

	void set_volume_control(int volume);
	int  get_volume_control();

	void set_PTT_control(int ptt_on);
	int  get_PTT();

	void tune_rig(int action);
	int  get_tune();

	void set_attenuator(int atten_on);
	int  get_attenuator();
	void set_preamp(int preamp);
	int  get_preamp();
	int  next_preamp();

	void set_if_shift(int shift);
	bool get_if_shift(int &shift);
	void get_if_min_max_step(int &min, int &max, int &step) {
		if_shift_min = min = -1200;
		if_shift_max = max = 1200;
		if_shift_step = step = 20;
		if_shift_mid = 0;
	}

	void set_notch(bool notch_on, int frequency);
	bool get_notch(int &frequency);
	void get_notch_min_max_step(int &min, int &max, int &step) {
		min = 10;
		max = 3200;
		step = 10;
	}

	void set_auto_notch(int notch_on);
	int  get_auto_notch();

/// noise blanker
	void set_noise(bool nb_on);
	int  get_noise();
	void get_nb_min_max_step(int &min, int &max, int &step) {
		min = 1;
		max = 10;
		step = 1;
	}
	void set_nb_level(int level);
	int  get_nb_level();

/// noise reduction (DNR)
	void set_noise_reduction_val(int level);
	int  get_noise_reduction_val();
	void get_nr_min_max_step(int &min, int &max, int &step) {
		min = 1;
		max = 10;
		step = 1;
	}

	void set_noise_reduction(int nr_on);
	int  get_noise_reduction();

	void set_mic_gain(int gain);
	int  get_mic_gain();
	void get_mic_min_max_step(int &min, int &max, int &step) {
		min = 0;
		max = 100;
		step = 1;
	}

	void set_rf_gain(int gain);
	int  get_rf_gain();
	void get_rf_min_max_step(int &min, int &max, int &step) {
		min = 0;
		max = 100;
		step = 1;
	}

	void set_squelch(int level);
	int  get_squelch();
	void get_squelch_min_max_step(int &min, int &max, int &step) {
		min = 0;
		max = 100;
		step = 5;
	}

	std::vector<std::string>& bwtable(int mode);

	void set_vox_onoff();
	void set_vox_gain();
	void set_vox_hang();
	void set_vox_on_dataport();

	void set_cw_weight();
	void set_cw_wpm();
	void get_cw_wpm_min_max(int &min, int &max) {
		min = 4;
		max = 60;
	}
	void enable_keyer();
	int  get_keyer();
	void set_cw_qsk();
	int  get_cw_qsk();
	void set_cw_delay();
	int  get_cw_delay();
	void set_cw_vol();
	int  get_cw_vol();

	bool set_cw_spot();
	void set_cw_spot_tone();
	void get_cw_spot_tone_min_max_step(int &min, int &max, int &step) {
		min = 300;
		max = 1050;
		step = 10;
	}

	void set_xcvr_auto_on();
	void set_xcvr_auto_off();

	void set_compression(int comp_on, int level);
	void get_compression(int &comp_on, int &level);
	void get_comp_min_max_step(int &min, int &max, int &step) {
		min = 0;
		max = 100;
		step = 5;
	}

	void get_band_selection(int band);

	void setVfoAdj(double adjust);
	double getVfoAdj();
	void get_vfoadj_min_max_step(double &min, double &max, double &step);

	void zero_in();
};

#endif // YAESU_FTX_1_H_INCLUDED
