/*	$NetBSD$	*/

/*-
 * Copyright (c) 2026 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the RK3568 lab (NetBSD/RTL8821CU bring-up).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * mac80211/cfg80211 shapes for the imported rtw88 chip code.
 *
 * These types exist so that 34k lines of Linux driver sources compile; the
 * driver itself never runs the mac80211 paths (no ieee80211_register_hw, no
 * rate control, no TXQ).  The members are only as complete as the chip code
 * actually touches, and the drv_priv[] areas are sized so that the chip
 * structures (rtw_vif, rtw_sta_info, rtw_txq) fit.
 *
 * net/mac80211.h and net/cfg80211.h both just include this file.
 */

#ifndef _RTW88_MAC80211_H_
#define _RTW88_MAC80211_H_

#include "rtw88_compat.h"

/* ------------------------------------------------------------------ */
/* constants                                                           */
/* ------------------------------------------------------------------ */

#define	IEEE80211_NUM_ACS	4
#define	IEEE80211_NUM_TIDS	16
#define	IEEE80211_NUM_BANDS	3
#define	NUM_NL80211_BANDS	IEEE80211_NUM_BANDS
#define	IEEE80211_MAX_SSID_LEN	32
#define	IEEE80211_MAX_DATA_LEN	2304
#define	IEEE80211_MAX_CHAINS	4
#define	IEEE80211_MAX_MPDU_LEN_VHT_11454	11454
#define	FCS_LEN					4
#define	IEEE80211_MAX_AMPDU_BUF_HT	256

enum nl80211_band {
	NL80211_BAND_2GHZ = 0,
	NL80211_BAND_5GHZ = 1,
	NL80211_BAND_60GHZ = 2,
};

enum nl80211_sar_type {
	NL80211_SAR_TYPE_POWER,
	NL80211_SAR_TYPE_COUNT
};

enum nl80211_iftype {
	NL80211_IFTYPE_UNSPECIFIED = 0,
	NL80211_IFTYPE_ADHOC = 1,
	NL80211_IFTYPE_STATION = 2,
	NL80211_IFTYPE_AP = 3,
	NL80211_IFTYPE_AP_VLAN = 4,
	NL80211_IFTYPE_WDS = 5,
	NL80211_IFTYPE_MONITOR = 6,
	NL80211_IFTYPE_MESH_POINT = 7,
	NL80211_IFTYPE_P2P_CLIENT = 8,
	NL80211_IFTYPE_P2P_GO = 9,
	NL80211_IFTYPE_P2P_DEVICE = 10,
};

enum nl80211_chan_width {
	NL80211_CHAN_WIDTH_20_NOHT = 0,
	NL80211_CHAN_WIDTH_20 = 1,
	NL80211_CHAN_WIDTH_40 = 2,
	NL80211_CHAN_WIDTH_80 = 3,
	NL80211_CHAN_WIDTH_80P80 = 4,
	NL80211_CHAN_WIDTH_160 = 5,
};

enum ieee80211_ac_numbers {
	IEEE80211_AC_VO = 0,
	IEEE80211_AC_VI = 1,
	IEEE80211_AC_BE = 2,
	IEEE80211_AC_BK = 3,
};

enum ieee80211_smps_mode {
	IEEE80211_SMPS_OFF = 0,
	IEEE80211_SMPS_STATIC = 1,
	IEEE80211_SMPS_DYNAMIC = 2,
	IEEE80211_SMPS_AUTOMATIC = 3,
};

enum ieee80211_sta_rx_bandwidth {
	IEEE80211_STA_RX_BW_20 = 0,
	IEEE80211_STA_RX_BW_40 = 1,
	IEEE80211_STA_RX_BW_80 = 2,
	IEEE80211_STA_RX_BW_160 = 3,
};

/* channel flags */
#define	IEEE80211_CHAN_NO_IR		BIT(4)
#define	IEEE80211_CHAN_RADAR		BIT(3)
/* IEEE80211_CHAN_NO_IBSS and _PASSIVE_SCAN are aliases of _NO_IR, defined
 * by the imported regd.h exactly as Linux does. */
#define	IEEE80211_CHAN_NO_HT40MINUS	BIT(7)
#define	IEEE80211_CHAN_NO_80MHZ		BIT(11)
#define	IEEE80211_CHAN_DISABLED		0

/* HT capability bits */
#define	IEEE80211_HT_CAP_LDPC_CODING			BIT(0)
#define	IEEE80211_HT_CAP_SUP_WIDTH_20_40		BIT(1)
#define	IEEE80211_HT_CAP_SGI_20				BIT(5)
#define	IEEE80211_HT_CAP_SGI_40				BIT(6)
#define	IEEE80211_HT_CAP_TX_STBC			BIT(7)
#define	IEEE80211_HT_CAP_RX_STBC			(BIT(8) | BIT(9))
#define	IEEE80211_HT_CAP_RX_STBC_SHIFT			8
#define	IEEE80211_HT_CAP_DSSSCCK40			BIT(10)
#define	IEEE80211_HT_CAP_MAX_AMSDU			BIT(11)
#define	IEEE80211_HT_MCS_TX_DEFINED			BIT(0)
#define	IEEE80211_HT_MAX_AMPDU_64K			3
#define	IEEE80211_HT_MPDU_DENSITY_2			2

/* VHT capability bits */
#define	IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454		BIT(1)
#define	IEEE80211_VHT_CAP_RXLDPC			BIT(4)
#define	IEEE80211_VHT_CAP_SHORT_GI_80			BIT(5)
#define	IEEE80211_VHT_CAP_RXSTBC_MASK			(BIT(8) | BIT(9) | BIT(10))
#define	IEEE80211_VHT_CAP_RXSTBC_1			BIT(8)
#define	IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE		BIT(11)
#define	IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE		BIT(12)
#define	IEEE80211_VHT_CAP_BEAMFORMEE_STS_SHIFT		13
#define	IEEE80211_VHT_CAP_BEAMFORMEE_STS_MASK		(BIT(13) | BIT(14) | BIT(15))
#define	IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_SHIFT	16
#define	IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_MASK	(BIT(16) | BIT(17) | BIT(18))
#define	IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE		BIT(19)
#define	IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE		BIT(20)
#define	IEEE80211_VHT_CAP_TXSTBC			BIT(26)
#define	IEEE80211_VHT_CAP_HTC_VHT			BIT(29)
#define	IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK BIT(31)
#define	IEEE80211_VHT_MCS_SUPPORT_0_7			0
#define	IEEE80211_VHT_MCS_SUPPORT_0_8			1
#define	IEEE80211_VHT_MCS_SUPPORT_0_9			2
#define	IEEE80211_VHT_MCS_NOT_SUPPORTED			3

/* RX status flags and rate encoding (rx.c fills struct ieee80211_rx_status) */
#define	RX_FLAG_MMIC_ERROR			BIT(0)
#define	RX_FLAG_DECRYPTED			BIT(1)
#define	RX_FLAG_FAILED_FCS_CRC			BIT(3)
#define	RX_FLAG_FAILED_PLCP_CRC			BIT(4)
#define	RX_FLAG_HT				BIT(6)
#define	RX_FLAG_40MHZ				BIT(7)
#define	RX_FLAG_VHT				BIT(11)
#define	RX_FLAG_MACTIME_START			BIT(12)
#define	RX_FLAG_NO_SIGNAL_VAL			BIT(15)
#define	RX_FLAG_NO_PSDU				BIT(29)

#define	RX_ENC_LEGACY				0
#define	RX_ENC_HT				1
#define	RX_ENC_VHT				2

/* ieee80211_hw flags: the chip code names them without the prefix */
enum ieee80211_hw_flags {
	IEEE80211_HW_HAS_RATE_CONTROL,
	IEEE80211_HW_RX_INCLUDES_FCS,
	IEEE80211_HW_HOST_BROADCAST_PS_BUFFERING,
	IEEE80211_HW_SIGNAL_UNSPEC,
	IEEE80211_HW_SIGNAL_DBM,
	IEEE80211_HW_NEED_DTIM_BEFORE_ASSOC,
	IEEE80211_HW_SPECTRUM_MGMT,
	IEEE80211_HW_AMPDU_AGGREGATION,
	IEEE80211_HW_SUPPORTS_PS,
	IEEE80211_HW_PS_NULLFUNC_STACK,
	IEEE80211_HW_SUPPORTS_DYNAMIC_PS,
	IEEE80211_HW_MFP_CAPABLE,
	IEEE80211_HW_WANT_MONITOR_VIF,
	IEEE80211_HW_NO_AUTO_VIF,
	IEEE80211_HW_SW_CRYPTO_CONTROL,
	IEEE80211_HW_SUPPORT_FAST_XMIT,
	IEEE80211_HW_REPORTS_TX_ACK_STATUS,
	IEEE80211_HW_CONNECTION_MONITOR,
	IEEE80211_HW_QUEUE_CONTROL,
	IEEE80211_HW_SUPPORTS_PER_STA_GTK,
	IEEE80211_HW_AP_LINK_PS,
	IEEE80211_HW_TX_AMSDU,
	IEEE80211_HW_TX_FRAG_LIST,
	IEEE80211_HW_REPORTS_LOW_ACK,
	IEEE80211_HW_SUPPORTS_TX_FRAG,
	IEEE80211_HW_SUPPORTS_AMSDU_IN_AMPDU,
	IEEE80211_HW_SINGLE_SCAN_ON_ALL_BANDS,
	NUM_IEEE80211_HW_FLAGS,
};

/* TX/RX status flags */
#define	IEEE80211_TX_CTL_AMPDU			BIT(5)
#define	IEEE80211_TX_CTL_REQ_TX_STATUS		BIT(10)
#define	IEEE80211_TX_CTL_NO_PS_BUFFER		BIT(3)
#define	IEEE80211_TX_STAT_ACK			BIT(13)

#define	IEEE80211_CONF_IDLE			BIT(3)

#define	IEEE80211_IFACE_ITER_NORMAL		0

#define	IEEE80211_KEY_FLAG_PAIRWISE		BIT(0)
#define	IEEE80211_KEY_FLAG_GENERATE_IV		BIT(1)
#define	IEEE80211_KEY_FLAG_GENERATE_MMIC	BIT(2)
#define	IEEE80211_KEY_FLAG_PUT_IV_SPACE		BIT(3)
#define	IEEE80211_KEY_FLAG_RX_MGMT		BIT(4)

#define	WLAN_CIPHER_SUITE_WEP40			0x000fac01
#define	WLAN_CIPHER_SUITE_TKIP			0x000fac02
#define	WLAN_CIPHER_SUITE_CCMP			0x000fac04
#define	WLAN_CIPHER_SUITE_WEP104		0x000fac05
#define	WLAN_CIPHER_SUITE_AES_CMAC		0x000fac06

#define	WIPHY_FLAG_SUPPORTS_TDLS		BIT(0)
#define	WIPHY_FLAG_TDLS_EXTERNAL_SETUP		BIT(1)
#define	WIPHY_FLAG_IBSS_RSN			BIT(2)

#define	NL80211_FEATURE_SCAN_RANDOM_MAC_ADDR	BIT(5)
#define	NL80211_EXT_FEATURE_CAN_REPLACE_PTK0	1
#define	NL80211_EXT_FEATURE_SCAN_RANDOM_SN	2
#define	NL80211_EXT_FEATURE_SET_SCAN_DWELL	3
#define	NL80211_SCAN_FLAG_RANDOM_ADDR		BIT(0)
#define	NL80211_SCAN_FLAG_RANDOM_SN		BIT(1)

#define	REGULATORY_STRICT_REG			BIT(0)
#define	REGULATORY_COUNTRY_IE_IGNORE		BIT(1)

#define	NL80211_REGDOM_SET_BY_DRIVER		0
#define	NL80211_REGDOM_SET_BY_USER		1
#define	NL80211_REGDOM_SET_BY_CORE		2

#define	NL80211_DFS_UNSET			0
#define	NL80211_DFS_FCC				1
#define	NL80211_DFS_ETSI			2
#define	NL80211_DFS_JP				3

#define	NL80211_SAR_TYPE_POWER			0

enum nl80211_cqm_rssi_threshold_event {
	NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW	= 0,
	NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH	= 1,
};

#define	RATE_INFO_FLAGS_MCS			BIT(0)
#define	RATE_INFO_FLAGS_SHORT_GI		BIT(1)
#define	RATE_INFO_FLAGS_VHT_MCS			BIT(2)
#define	RATE_INFO_BW_20				0
#define	RATE_INFO_BW_40				1
#define	RATE_INFO_BW_80				2

#define	RATE_INFO_FLAGS_60G			BIT(0)

#define	BSS_CHANGED_ASSOC			BIT(0)
#define	BSS_CHANGED_BASIC_RATES			BIT(1)

/* 802.11 frame control */
#define	IEEE80211_FCTL_FTYPE		0x000c
#define	IEEE80211_FCTL_STYPE		0x00f0
#define	IEEE80211_FCTL_TODS		0x0100
#define	IEEE80211_FCTL_FROMDS		0x0200
#define	IEEE80211_FTYPE_MGMT		0x0000
#define	IEEE80211_FTYPE_CTL		0x0004
#define	IEEE80211_FTYPE_DATA		0x0008
#define	IEEE80211_STYPE_BEACON		0x0080
#define	IEEE80211_STYPE_PROBE_RESP	0x0050
#define	IEEE80211_STYPE_QOS_NULLFUNC	0x00c0
#define	IEEE80211_STYPE_NULLFUNC	0x0040
#define	IEEE80211_SCTL_SEQ		0xfff0

#define	IEEE80211_HDRLEN		24

#define	IEEE80211_SKB_CB(skb)	((struct ieee80211_tx_info *)((skb)->cb))

/* ------------------------------------------------------------------ */
/* 802.11 headers                                                      */
/* ------------------------------------------------------------------ */

struct ieee80211_hdr {
	__le16		frame_control;
	__le16		duration_id;
	u8		addr1[6];
	u8		addr2[6];
	u8		addr3[6];
	__le16		seq_ctrl;
	u8		addr4[6];
} __packed;

struct ieee80211_hdr_3addr {
	__le16		frame_control;
	__le16		duration_id;
	u8		addr1[6];
	u8		addr2[6];
	u8		addr3[6];
	__le16		seq_ctrl;
} __packed;

struct ieee80211_mgmt {
	__le16		frame_control;
	__le16		duration;
	u8		da[6];
	u8		sa[6];
	u8		bssid[6];
	__le16		seq_ctrl;
	union {
		struct {
			__le64	timestamp;
			__le16	beacon_int;
			__le16	capab_info;
			u8	variable[1];
		} __packed beacon;
		struct {
			__le64	timestamp;
			__le16	beacon_int;
			__le16	capab_info;
			u8	variable[1];
		} __packed probe_resp;
		u8		variable[1];
	} u;
} __packed;

static __always_inline __unused int
ieee80211_is_mgmt(__le16 fc)
{
	return (le16toh(fc) & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_MGMT;
}

static __always_inline __unused int
ieee80211_is_data(__le16 fc)
{
	return (le16toh(fc) & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_DATA;
}

static __always_inline __unused int
ieee80211_is_ctl(__le16 fc)
{
	return (le16toh(fc) & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_CTL;
}

static __always_inline __unused int
ieee80211_is_beacon(__le16 fc)
{
	return (le16toh(fc) & (IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	    (IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
}

static __always_inline __unused int
ieee80211_is_probe_resp(__le16 fc)
{
	return (le16toh(fc) & (IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	    (IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_RESP);
}

static __always_inline __unused int
ieee80211_is_any_nullfunc(__le16 fc)
{
	u16 v = le16toh(fc) & (IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE);

	return v == (IEEE80211_FTYPE_DATA | IEEE80211_STYPE_NULLFUNC) ||
	    v == (IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_NULLFUNC);
}

static __always_inline __unused int
ieee80211_has_tods(__le16 fc)
{
	return (le16toh(fc) & IEEE80211_FCTL_TODS) != 0;
}

static __always_inline __unused int
ieee80211_has_fromds(__le16 fc)
{
	return (le16toh(fc) & IEEE80211_FCTL_FROMDS) != 0;
}

/* ------------------------------------------------------------------ */
/* cfg80211 types                                                      */
/* ------------------------------------------------------------------ */

struct ieee80211_channel {
	u16			center_freq;
	u16			hw_value;
	u32			flags;
	u8			band;
	u8			max_antenna_gain;
	s8			max_power;
	s32			max_reg_power;
	bool			beacon_found;
};

struct ieee80211_rate {
	u32			flags;
	u16			bitrate;
	u16			hw_value;
	u16			hw_value_short;
};

enum nl80211_bss_scan_width {
	NL80211_BSS_CHAN_WIDTH_20 = 0,
};

struct ieee80211_sta_ht_cap {
	u16			cap;
	bool			ht_supported;
	u8			ampdu_factor;
	u8			ampdu_density;
	struct {
		u8		rx_mask[10];
		u16		rx_highest;
		u8		tx_params;
	} mcs;
};

struct ieee80211_sta_vht_cap {
	bool			vht_supported;
	u32			cap;
	struct {
		__le16		rx_mcs_map;
		__le16		tx_mcs_map;
		__le16		rx_highest;
		__le16		tx_highest;
	} vht_mcs;
};

struct ieee80211_supported_band {
	const struct ieee80211_channel *channels;
	const struct ieee80211_rate *bitrates;
	enum nl80211_band	band;
	int			n_channels;
	int			n_bitrates;
	struct ieee80211_sta_ht_cap ht_cap;
	struct ieee80211_sta_vht_cap vht_cap;
};

struct cfg80211_chan_def {
	struct ieee80211_channel *chan;
	enum nl80211_chan_width	width;
	u32			center_freq1;
	u32			center_freq2;
};

struct cfg80211_ssid {
	u8			ssid[IEEE80211_MAX_SSID_LEN];
	u8			ssid_len;
};

struct cfg80211_bitrate_mask {
	struct {
		u32		legacy;
		u8		ht_mcs[10];
		u16		vht_mcs[8];
	} control[IEEE80211_NUM_BANDS];
};

struct cfg80211_sar_freq_ranges {
	u32			start_freq;
	u32			end_freq;
};

struct cfg80211_sar_capa {
	enum nl80211_sar_type	type;
	u32			num_freq_ranges;
	const struct cfg80211_sar_freq_ranges *freq_ranges;
};

struct cfg80211_sar_sub_specs {
	u32			freq_range_index;
	s32			power;
};

struct cfg80211_sar_specs {
	enum nl80211_sar_type	type;
	u32			num_sub_specs;
	const struct cfg80211_sar_sub_specs *sub_specs;
};

struct cfg80211_scan_info {
	u64			scan_start_tsf;
	u8			tsf_bssid[6];
	bool			aborted;
};

struct cfg80211_sched_scan_plan {
	u32			interval;
	u32			iterations;
};

struct cfg80211_match_set {
	struct cfg80211_ssid	ssid;
	u8			bssid[6];
	s32			rssi_thold;
	s32			per_band_rssi_thold[IEEE80211_NUM_BANDS];
};

struct cfg80211_scan_request {
	struct cfg80211_ssid	*ssids;
	struct ieee80211_channel **channels;
	u8			n_ssids;
	u32			n_channels;
	u32			flags;
	u32			duration;
	bool			duration_mandatory;
	bool			no_cck;
	size_t			ie_len;
	u8			mac_addr[6];
	u8			mac_addr_mask[6];
};

struct cfg80211_wowlan {
	bool			any;
	bool			disconnect;
	bool			magic_pkt;
	bool			gtk_rekey_failure;
	bool			eap_identity_req;
	bool			four_way_handshake;
	bool			rfkill_release;
	struct cfg80211_sched_scan_plan *patterns;
	bool			tcp;
	bool			net_detect;
};

struct wiphy_wowlan_support {
	u32			flags;
	int			n_patterns;
	int			pattern_max_len;
	int			pattern_min_len;
	int			max_pkt_offset;
};

struct ieee80211_iface_limit {
	u16			max;
	u16			types;
};

struct ieee80211_iface_combination {
	const struct ieee80211_iface_limit *limits;
	u32			num_different_channels;
	u16			max_interfaces;
	u8			n_limits;
	bool			beacon_int_infra_match;
	u8			radar_detect_widths;
	u8			radar_detect_regions;
};

struct ieee80211_tx_queue_params {
	u16			aifs;
	u16			cw_min;
	u16			cw_max;
	u16			txop;
	bool			acm;
	bool			uapsd;
};

struct ieee80211_scan_ies {
	const u8		*ies[IEEE80211_NUM_BANDS];
	size_t			len[IEEE80211_NUM_BANDS];
	const u8		*common_ies;
	size_t			common_ie_len;
};

struct ieee80211_scan_request {
	struct cfg80211_scan_request req;
	struct list_head	scan_list;
	struct ieee80211_scan_ies ies;
};

enum nl80211_dfs_regions {
	NL80211_DFS_REGION_UNSET = NL80211_DFS_UNSET,
	NL80211_DFS_REGION_FCC = NL80211_DFS_FCC,
	NL80211_DFS_REGION_ETSI = NL80211_DFS_ETSI,
	NL80211_DFS_REGION_JP = NL80211_DFS_JP,
};

struct regulatory_request {
	char			alpha2[2];
	enum nl80211_dfs_regions dfs_region;
	u32			initiator;
};

/*
 * The wiphy is large in Linux; only the members the chip code touches are
 * present.  ieee80211_hw embeds one so that hw->wiphy is always valid.
 */
struct wiphy {
	struct ieee80211_supported_band *bands[IEEE80211_NUM_BANDS];
	u32			flags;
	u32			features;
	u32			regulatory_flags;
	u16			interface_modes;
	u32			available_antennas_tx;
	u32			available_antennas_rx;
	int			max_scan_ssids;
	int			max_sched_scan_ssids;
	u16			max_scan_ie_len;
	u32			rts_threshold;
	u8			fw_version[32];
	void			(*reg_notifier)(struct wiphy *,
					    struct regulatory_request *);
	const struct ieee80211_iface_combination *iface_combinations;
	int			n_iface_combinations;
	const struct cfg80211_sar_capa *sar_capa;
	const struct wiphy_wowlan_support *wowlan;
	void			*debugfsdir;
	u8			perm_addr[ETH_ALEN];
};

/* ------------------------------------------------------------------ */
/* mac80211 types                                                      */
/* ------------------------------------------------------------------ */

struct ieee80211_conf {
	u32			flags;
	int			power_level;
	int			max_power_level;
	struct cfg80211_chan_def chandef;
};

/* enough of mac80211's rate_info for the RA report */
struct rate_info {
	u8			flags;
	u8			legacy;
	u8			mcs;
	u8			nss;
	u8			bw;
	u8			he_gi;
	u16			he_ru_alloc;
};

struct ieee80211_bss_conf {
	u8			bssid[ETH_ALEN];
	bool			assoc;
	u32			basic_rates;
	u16			aid;
	u16			beacon_int;
	bool			enable_beacon;
	s32			cqm_rssi_thold;
	u8			cqm_rssi_hyst;
	struct {
		u8		membership[8];
		u8		position[16];
	} mu_group;
};

struct ieee80211_tx_control {
	struct ieee80211_sta	*sta;
	struct ieee80211_txq	*txq;
};

struct ieee80211_key_conf {
	u32			cipher;
	u8			icv_len;
	u8			iv_len;
	u8			hw_key_idx;
	u8			keyidx;
	u32			flags;
	u8			keylen;
	u8			key[32];
};

struct ieee80211_hw {
	struct ieee80211_conf	conf;
	struct wiphy		*wiphy;
	const struct ieee80211_ops *ops;
	void			*priv;
	unsigned long		flags;
	u32			extra_tx_headroom;
	u32			queues;
	int			txq_data_size;
	int			sta_data_size;
	int			vif_data_size;
};

struct ieee80211_vif {
	enum nl80211_iftype	type;
	u8			addr[ETH_ALEN];
	struct ieee80211_bss_conf bss_conf;
	struct {
		bool		assoc;
		bool		ps;
		u16		aid;
	} cfg;
	struct ieee80211_txq	*txq;
	u8			drv_priv[128] __aligned(sizeof(void *));
};

struct ieee80211_sta {
	u8			addr[ETH_ALEN];
	u16			aid;
	bool			tdls;
	u8			bandwidth;
	u8			max_rc_amsdu_len;
	struct ieee80211_sta_ht_cap ht_cap;
	struct ieee80211_sta_vht_cap vht_cap;
	struct ieee80211_txq	*txq[IEEE80211_NUM_TIDS];
	u8			supp_rates[IEEE80211_NUM_BANDS];
	struct {
		u8		bandwidth;
		struct {
			u16	max_rc_amsdu_len;
		} agg;
		struct ieee80211_sta_ht_cap ht_cap;
		struct ieee80211_sta_vht_cap vht_cap;
		u8		supp_rates[IEEE80211_NUM_BANDS];
	} deflink;
	u8			drv_priv[128] __aligned(sizeof(void *));
};

struct ieee80211_txq {
	u8			ac;
	u8			tid;
	struct ieee80211_sta	*sta;
	struct ieee80211_vif	*vif;
	u8			drv_priv[64] __aligned(sizeof(void *));
};

struct ieee80211_tx_info {
	u32			flags;
	u8			band;
	u8			hw_queue;
	struct {
		struct ieee80211_key_conf *hw_key;
		struct ieee80211_vif *vif;
		struct ieee80211_sta *sta;
		bool		use_rts;
		bool		use_cts_prot;
		u8		flags;
	} control;
	struct {
		u8		status_driver_data[18];
		u8		rates[4];
		u8		acked;
	} status;
	void			*driver_data[8];
};

struct ieee80211_rx_status {
	u64			mactime;
	u64			boottime_ns;
	u32			device_timestamp;
	u16			freq;
	u16			flag;
	u8			band;
	u8			encoding;
	u8			bw;
	u8			nss;
	u8			chains;
	s8			signal;
	s8			chain_signal[IEEE80211_MAX_CHAINS];
	u8			rate_idx;
	s8			vht_flag;
	u8			zero_length_psdu_type;
};

/* Only the type has to exist: the driver never registers a mac80211 ops. */
struct ieee80211_ops {
	int			dummy;
};

/* ------------------------------------------------------------------ */
/* mac80211 helpers                                                    */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* mac80211 entry points the chip code calls; implemented in           */
/* rtw88_compat.c.  Only the ones on the driver's runtime path do      */
/* anything (queue_work, the iterate helpers, find_sta); the rest are  */
/* here so the chip code links.                                        */
/* ------------------------------------------------------------------ */

struct ieee80211_hw *wiphy_to_ieee80211_hw(struct wiphy *);
int	regulatory_hint(struct wiphy *, const char *);
void	get_random_mask_addr(u8 *, const u8 *, const u8 *);

int	ieee80211_register_hw(struct ieee80211_hw *);
void	ieee80211_unregister_hw(struct ieee80211_hw *);
void	ieee80211_restart_hw(struct ieee80211_hw *);
void	ieee80211_queue_work(struct ieee80211_hw *, struct work_struct *);
void	ieee80211_queue_delayed_work(struct ieee80211_hw *,
	    struct delayed_work *, unsigned long);
void	ieee80211_wake_queues(struct ieee80211_hw *);
void	ieee80211_stop_queues(struct ieee80211_hw *);
void	ieee80211_tx_status_irqsafe(struct ieee80211_hw *, struct sk_buff *);
void	ieee80211_free_txskb(struct ieee80211_hw *, struct sk_buff *);
void	ieee80211_tx_info_clear_status(struct ieee80211_tx_info *);
struct sk_buff *ieee80211_tx_dequeue(struct ieee80211_hw *,
	    struct ieee80211_txq *);
int	ieee80211_txq_get_depth(struct ieee80211_txq *, unsigned long *,
	    unsigned long *);
struct ieee80211_sta *ieee80211_find_sta(struct ieee80211_vif *,
	    const u8 *);
struct ieee80211_sta *ieee80211_find_sta_by_ifaddr(struct ieee80211_hw *,
	    const u8 *, const u8 *);
void	ieee80211_iterate_active_interfaces_atomic(struct ieee80211_hw *,
	    u32, void (*)(void *, u8 *, struct ieee80211_vif *), void *);
void	ieee80211_iterate_active_interfaces(struct ieee80211_hw *, u32,
	    void (*)(void *, u8 *, struct ieee80211_vif *), void *);
void	ieee80211_iterate_stations_atomic(struct ieee80211_hw *,
	    void (*)(void *, struct ieee80211_sta *), void *);
void	ieee80211_iterate_stations(struct ieee80211_hw *,
	    void (*)(void *, struct ieee80211_sta *), void *);
void	ieee80211_iter_keys(struct ieee80211_hw *, struct ieee80211_vif *,
	    void (*)(struct ieee80211_hw *, struct ieee80211_vif *,
	    struct ieee80211_sta *, struct ieee80211_key_conf *, void *),
	    void *);
void	ieee80211_iter_keys_rcu(struct ieee80211_hw *, struct ieee80211_vif *,
	    void (*)(struct ieee80211_hw *, struct ieee80211_vif *,
	    struct ieee80211_sta *, struct ieee80211_key_conf *, void *),
	    void *);
void	ieee80211_request_smps(struct ieee80211_vif *, int,
	    enum ieee80211_smps_mode);
int	ieee80211_start_tx_ba_session(struct ieee80211_sta *, u16, u16);
void	ieee80211_scan_completed(struct ieee80211_hw *,
	    struct cfg80211_scan_info *);
void	ieee80211_connection_loss(struct ieee80211_vif *);
void	ieee80211_cqm_rssi_notify(struct ieee80211_vif *,
	    enum nl80211_cqm_rssi_threshold_event, s32, gfp_t);
struct sk_buff *ieee80211_nullfunc_get(struct ieee80211_hw *,
	    struct ieee80211_vif *, int, bool);
struct sk_buff *ieee80211_probereq_get(struct ieee80211_hw *, const u8 *,
	    const u8 *, size_t, size_t);
struct sk_buff *ieee80211_proberesp_get(struct ieee80211_hw *,
	    struct ieee80211_vif *);
struct sk_buff *ieee80211_pspoll_get(struct ieee80211_hw *,
	    struct ieee80211_vif *);
struct sk_buff *ieee80211_beacon_get_tim(struct ieee80211_hw *,
	    struct ieee80211_vif *, u16 *, u16 *, int);



static __always_inline __unused void
ieee80211_hw_set_flag(struct ieee80211_hw *hw, unsigned int flag)
{
	set_bit(flag, &hw->flags);
}

#define	ieee80211_hw_set(hw, _name)	\
	ieee80211_hw_set_flag((hw), IEEE80211_HW_##_name)

static __always_inline __unused void
wiphy_ext_feature_set(struct wiphy *wiphy, unsigned int feature)
{
	set_bit(feature, (unsigned long *)&wiphy->features);
}

struct ieee80211_hw *wiphy_to_ieee80211_hw(struct wiphy *);
int	regulatory_hint(struct wiphy *, const char *);
void	get_random_mask_addr(u8 *, const u8 *, const u8 *);

#define	SET_IEEE80211_DEV(hw, dev)	do { } while (0)
#define	SET_IEEE80211_PERM_ADDR(hw, addr)				\
	do {								\
		if ((hw)->wiphy != NULL)				\
			memcpy((hw)->wiphy->perm_addr, (addr), ETH_ALEN); \
	} while (0)

static __always_inline __unused int
ieee80211_channel_to_frequency(int chan, enum nl80211_band band)
{
	if (band == NL80211_BAND_2GHZ) {
		if (chan == 14)
			return 2484;
		return 2407 + chan * 5;
	}
	return 5000 + chan * 5;
}

static __always_inline __unused int
cfg80211_get_ies_channel_number(const u8 *ies, size_t ielen,
    enum nl80211_band band)
{
	return 1;
}

static __always_inline __unused u32
cfg80211_calculate_bitrate(void *rate)
{
	return 0;
}

static __always_inline __unused bool
cfg80211_ssid_eq(struct cfg80211_ssid *a, struct cfg80211_ssid *b)
{
	if (a == NULL || b == NULL || a->ssid_len != b->ssid_len)
		return false;
	return memcmp(a->ssid, b->ssid, a->ssid_len) == 0;
}

#endif /* _RTW88_MAC80211_H_ */
