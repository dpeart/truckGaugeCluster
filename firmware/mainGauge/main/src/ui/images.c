#include "images.h"

const ext_img_desc_t images[6] = {
    { "center", &img_center },
    { "Right", &img_right },
    { "Left", &img_left },
    { "HighBeam", &img_high_beam },
    { "mainGaugeBG", &img_main_gauge_bg },
    { "centerGlowRing", &img_center_glow_ring },
};