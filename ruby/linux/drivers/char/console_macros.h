#define cons_num	(vc->vc_num)
#define video_num_columns  (vc->vc_cols)
#define video_num_lines (vc->vc_rows)
#define video_size_row  (vc->vc_size_row)
#define can_do_color    (vc->vc_can_do_color)
#define screenbuf	(vc->vc_screenbuf)
#define screenbuf_size	(vc->vc_screenbuf_size)
#define origin		(vc->vc_origin)
#define scr_top		(vc->vc_scr_top)
#define visible_origin  (vc->vc_visible_origin)
#define scr_end		(vc->vc_scr_end)
#define pos		(vc->vc_pos)
#define top		(vc->vc_top)
#define bottom		(vc->vc_bottom)
#define x		(vc->vc_x)
#define y		(vc->vc_y)
#define vc_state	(vc->vc_state)
#define npar		(vc->vc_npar)
#define par		(vc->vc_par)
#define ques		(vc->vc_ques)
#define attr		(vc->vc_attr)
#define saved_x		(vc->vc_saved_x)
#define saved_y		(vc->vc_saved_y)
#define translate	(vc->vc_translate)
#define G0_charset	(vc->vc_G0_charset)
#define G1_charset	(vc->vc_G1_charset)
#define saved_G0	(vc->vc_saved_G0)
#define saved_G1	(vc->vc_saved_G1)
#define utf		(vc->vc_utf)
#define utf_count	(vc->vc_utf_count)
#define utf_char	(vc->vc_utf_char)
#define video_erase_char (vc->vc_video_erase_char)
#define disp_ctrl	(vc->vc_disp_ctrl)
#define toggle_meta	(vc->vc_toggle_meta)
#define decscnm		(vc->vc_decscnm)
#define decom		(vc->vc_decom)
#define decawm		(vc->vc_decawm)
#define deccm		(vc->vc_deccm)
#define decim		(vc->vc_decim)
#define deccolm		(vc->vc_deccolm)
#define need_wrap	(vc->vc_need_wrap)
#define kmalloced	(vc->vc_kmalloced)
#define report_mouse	(vc->vc_report_mouse)
#define color		(vc->vc_color)
#define s_color		(vc->vc_s_color)
#define def_color	(vc->vc_def_color)
#define foreground	(color & 0x0f)
#define background	(color & 0xf0)
#define charset		(vc->vc_charset)
#define s_charset	(vc->vc_s_charset)
#define	intensity	(vc->vc_intensity)
#define	underline	(vc->vc_underline)
#define	blink		(vc->vc_blink)
#define	reverse		(vc->vc_reverse)
#define	s_intensity	(vc->vc_s_intensity)
#define	s_underline	(vc->vc_s_underline)
#define	s_blink		(vc->vc_s_blink)
#define	s_reverse	(vc->vc_s_reverse)
#define	ulcolor		(vc->vc_ulcolor)
#define	halfcolor	(vc->vc_halfcolor)
#define tab_stop	(vc->vc_tab_stop)
#define palette		(vc->vc_palette)
#define bell_pitch	(vc->vc_bell_pitch)
#define bell_duration	(vc->vc_bell_duration)
#define cursor_type	(vc->vc_cursor_type)
#define complement_mask (vc->vc_complement_mask)
#define s_complement_mask (vc->vc_s_complement_mask)
#define hi_font_mask	(vc->vc_hi_font_mask)

#define softcursor_original (vc->display_fg->cursor_original)
#define vcmode		(vc->display_fg->vc_mode)
#define sw              (vc->display_fg->vt_sw)

#define dectcem 	(vc->vc_dectcem)
#define decscl		(vc->vc_decscl)
#define c8bit		(vc->vc_c8bit)
#define d8bit           (vc->vc_d8bit)
#define shift                (vc->vc_shift)
#define priv1                (vc->vc_priv1)
#define priv2                (vc->vc_priv2)
#define priv3                (vc->vc_priv3)
#define priv4                (vc->vc_priv4)
#define decckm                (vc->vc_decckm)
#define decsclm                (vc->vc_decsclm)
#define decarm                (vc->vc_decarm)
#define decnrcm                (vc->vc_decnrcm)
#define decnkm                (vc->vc_decnkm)
#define kam                (vc->vc_kam)
#define crm                (vc->vc_crm)
#define lnm                (vc->vc_lnm)
#define irm 		 (vc->vc_irm)
#define GL_charset                (vc->vc_GL_charset)
#define GR_charset                (vc->vc_GR_charset)
#define G2_charset                (vc->vc_G2_charset)
#define G3_charset                (vc->vc_G3_charset)
#define GS_charset                (vc->vc_GS_charset)
#define saved_G2                (vc->vc_saved_G2)
#define saved_G3                (vc->vc_saved_G3)
#define saved_GS                (vc->vc_saved_GS)
