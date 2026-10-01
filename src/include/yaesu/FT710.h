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

#ifndef YAESU_FT710_H_INCLUDED
#define YAESU_FT710_H_INCLUDED

#include "rigbase.h"

/// Yaesu FT-710, CAT Operation Reference Manual
class RIG_FT710 : public rigbase {
private:
/// manual notch state last read
	bool m_notch_on;
/// 60 m combo entry last selected
	int  m_60m_indx;

/// last good raw reading (0-255) of each RM meter, indexed by RM P1
	int  m_meter_raw[9];

	size_t last_frame(const char *prefix, size_t length);
	bool fixed_width(int mode);
	int  read_meter(int meter, const char *label);

public:
	RIG_FT710();
	~RIG_FT710() {}

	void initialize();

	bool check();

	unsigned long long get_vfoA();
	void set_vfoA(unsigned long long frequency);

	unsigned long long get_vfoB();
	void set_vfoB(unsigned long long frequency);

	int  get_vfoAorB();

	bool twovfos();
	void selectA();
	void selectB();
	void A2B();
	bool can_split();
	void set_split(bool split_on);
	int  get_split();

	void swapAB();
	bool canswap() {
		return true;
	}

	void set_modeA(int mode);
	int  get_modeA();
	int  get_modetype(int mode);

	void set_modeB(int mode);
	int  get_modeB();

	void set_bwA(int bw_index);
	int  get_bwA();

	void set_bwB(int bw_index);
	int  get_bwB();

	int  adjust_bandwidth(int mode);
	int  def_bandwidth(int mode);

	void set_BANDWIDTHS(std::string widths);
	std::string get_BANDWIDTHS();

	int  get_smeter();
	int  get_swr();
	int  get_alc();
	double get_idd();
	double get_voltmeter();

	int  get_power_out();
	double get_power_control();
	void set_power_control(double watts);
	void get_pc_min_max_step(double &min, double &max, double &step) {
		min = 5;
		pmax = max = 100;
		step = 1;
	}

	void set_squelch(int level);
	int  get_squelch();
	void get_squelch_min_max_step(int &min, int &max, int &step) {
		min = 0;
		max = 100;
		step = 5;
	}

	void set_volume_control(int volume);
	int  get_volume_control();
	void set_PTT_control(int ptt_on);
	int  get_PTT();
	void tune_rig(int action);
	int  get_tune();

	int  next_attenuator();
	void set_attenuator(int atten);
	int  get_attenuator();
	int  next_preamp();
	void set_preamp(int preamp);
	int  get_preamp();

	void set_if_shift(int shift);
	bool get_if_shift(int &shift);
	void get_if_min_max_step(int &min, int &max, int &step);

	void set_notch(bool notch_on, int frequency);
	bool get_notch(int &frequency);
	void get_notch_min_max_step(int &min, int &max, int &step);

	void set_auto_notch(int notch_on);
	int  get_auto_notch();

/// noise blanker
	void set_noise(bool nb_on);
	int  get_noise();

	void set_mic_gain(int gain);
	int  get_mic_gain();
	void get_mic_min_max_step(int &min, int &max, int &step);

	void set_rf_gain(int gain);
	int  get_rf_gain();
	void get_rf_min_max_step(int &min, int &max, int &step);
	std::vector<std::string>& bwtable(int mode);

	void set_vox_onoff();
	void set_vox_gain();
	void set_vox_anti();
	void get_vox_anti_min_max_step(int &min, int &max, int &step) {
		min = 1;
		max = 100;
		step = 1;
	}
	void get_vox_anti_min_max_step(double &min, double &max, double &step) {
		min = 1;
		max = 100;
		step = 1;
	}
	void set_vox_hang();
	void set_vox_on_dataport();

	void get_cw_wpm_min_max(int &min, int &max) {
		min = 4;
		max = 60;
	}

	void set_cw_weight();
	void set_cw_wpm();
	void enable_keyer();
	void set_cw_qsk();
	bool set_cw_spot();
	void set_break_in();
	int  get_break_in();

	void get_band_selection(int band);

/// noise reduction (DNR)
	void get_nr_min_max_step(int &min, int &max, int &step) {
		min = 1;
		max = 15;
		step = 1;
	}
	void set_noise_reduction_val(int level);
	int  get_noise_reduction_val();
	void set_noise_reduction(int nr_on);
	int  get_noise_reduction();

	void set_xcvr_auto_on();
	void set_xcvr_auto_off();

	void sync_date(char *date_str);
	void sync_clock(char *time_str);
};

#endif // YAESU_FT710_H_INCLUDED
