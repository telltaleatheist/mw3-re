# Module map of Mech3.exe 1.2

How this was built: debug assertions in the exe carry the original source paths
(`C:\zipper\Mech3\weapon.cpp`, `C:\zipper\GameZMech3\zVideo\zvid_dd.c`, ...).
Each function that references one belongs to that file, and the linker placed each
file's functions together in link order, so the references give address ranges.
Rows in parentheses are gaps between two known files: one or more files with no
assertion strings. The function-to-file list is `analysis-output/func2file.json`
(regenerate with the scratch tools; see CLAUDE.md).

Two halves: the game (`C:\zipper\Mech3\`, 0x401000-0x4eada0) and Zipper's
GameZ engine (`C:\zipper\GameZMech3\`, 0x4eaea0-0x5aa3b0). Everything after
0x5aa450 (2,903 small functions) is the C runtime and other libraries.

| Address range | Functions | Pseudocode | Original file |
|---|---:|---:|---|
| `0x401000-0x406440` | 101 | 87 KB | (between start and mech3/briefing.cpp) |
| `0x406650-0x406650` | 1 | 23 KB | `mech3/briefing.cpp` |
| `0x407c90-0x4143c0` | 397 | 207 KB | (between mech3/briefing.cpp and mech3/mech.cpp) |
| `0x414430-0x415b00` | 10 | 28 KB | `mech3/mech.cpp` |
| `0x415c70-0x41f180` | 117 | 136 KB | (between mech3/mech.cpp and mech3/mech_anim.cpp) |
| `0x41f340-0x41f340` | 1 | 1 KB | `mech3/mech_anim.cpp` |
| `0x41f580-0x430840` | 310 | 291 KB | (between mech3/mech_anim.cpp and mech3/mech_data.cpp) |
| `0x4308a0-0x431a70` | 21 | 25 KB | `mech3/mech_data.cpp` |
| `0x431fb0-0x458660` | 634 | 608 KB | (between mech3/mech_data.cpp and mech3/mech_map.cpp) |
| `0x4586e0-0x4586e0` | 1 | 0 KB | `mech3/mech_map.cpp` |
| `0x4587b0-0x45c4a0` | 68 | 67 KB | (between mech3/mech_map.cpp and mech3/mech_objective.cpp) |
| `0x45c810-0x45c810` | 1 | 1 KB | `mech3/mech_objective.cpp` |
| `0x45c8f0-0x461d30` | 73 | 122 KB | (between mech3/mech_objective.cpp and mech3/mech_parts.cpp) |
| `0x461d80-0x461d80` | 1 | 3 KB | `mech3/mech_parts.cpp` |
| `0x462000-0x46a860` | 122 | 131 KB | (between mech3/mech_parts.cpp and mech3/mech_pocket.cpp) |
| `0x46a930-0x46a930` | 1 | 0 KB | `mech3/mech_pocket.cpp` |
| `0x46aa40-0x46ba20` | 12 | 11 KB | (between mech3/mech_pocket.cpp and mech3/mech_reader.cpp) |
| `0x46ba40-0x46ba40` | 1 | 24 KB | `mech3/mech_reader.cpp` |
| `0x46cb80-0x473570` | 93 | 109 KB | (between mech3/mech_reader.cpp and mech3/mech_salvage.cpp) |
| `0x4735f0-0x4735f0` | 1 | 1 KB | `mech3/mech_salvage.cpp` |
| `0x473860-0x474500` | 18 | 21 KB | (between mech3/mech_salvage.cpp and mech3/mech_skin.cpp) |
| `0x474740-0x474740` | 1 | 3 KB | `mech3/mech_skin.cpp` |
| `0x4748b0-0x4797c0` | 81 | 106 KB | (between mech3/mech_skin.cpp and mech3/mech_waynet.cpp) |
| `0x479850-0x479850` | 1 | 0 KB | `mech3/mech_waynet.cpp` |
| `0x4798f0-0x48f630` | 429 | 332 KB | (between mech3/mech_waynet.cpp and mech3/mission.cpp) |
| `0x48f6c0-0x491de0` | 31 | 48 KB | `mech3/mission.cpp` |
| `0x492660-0x49dc40` | 171 | 193 KB | (between mech3/mission.cpp and mech3/player.cpp) |
| `0x49dc80-0x49fa20` | 16 | 43 KB | `mech3/player.cpp` |
| `0x49fce0-0x4a6230` | 98 | 119 KB | (between mech3/player.cpp and mech3/turret.cpp) |
| `0x4a6270-0x4a76b0` | 8 | 25 KB | `mech3/turret.cpp` |
| `0x4a7970-0x4ad840` | 98 | 115 KB | (between mech3/turret.cpp and mech3/veh_basic.cpp) |
| `0x4ad930-0x4ad930` | 1 | 2 KB | `mech3/veh_basic.cpp` |
| `0x4adaa0-0x4b44d0` | 151 | 128 KB | (between mech3/veh_basic.cpp and mech3/weapon.cpp) |
| `0x4b4660-0x4b4660` | 1 | 2 KB | `mech3/weapon.cpp` |
| `0x4b4880-0x4bcb70` | 178 | 144 KB | (between mech3/weapon.cpp and mech3/zainetedit.cpp) |
| `0x4bcba0-0x4bf1b0` | 34 | 56 KB | `mech3/zainetedit.cpp` |
| `0x4bf3f0-0x4c1160` | 60 | 37 KB | (between mech3/zainetedit.cpp and mech3/zainetmechinout.cpp) |
| `0x4c11f0-0x4c3ed0` | 67 | 43 KB | `mech3/zainetmechinout.cpp` |
| `0x4c3f70-0x4d2660` | 385 | 267 KB | (between mech3/zainetmechinout.cpp and mech3/zaitarget.cpp) |
| `0x4d26f0-0x4d26f0` | 1 | 1 KB | `mech3/zaitarget.cpp` |
| `0x4d27d0-0x4da8f0` | 229 | 167 KB | (between mech3/zaitarget.cpp and mech3/zrdrainet.cpp) |
| `0x4da900-0x4db8d0` | 11 | 33 KB | `mech3/zrdrainet.cpp` |
| `0x4dc150-0x4de980` | 45 | 61 KB | (between mech3/zrdrainet.cpp and mech3/zrdraiv.cpp) |
| `0x4de9c0-0x4e08c0` | 43 | 41 KB | `mech3/zrdraiv.cpp` |
| `0x4e0c20-0x4e2330` | 25 | 52 KB | (between mech3/zrdraiv.cpp and mech3/zrdrvalve.cpp) |
| `0x4e2fb0-0x4e44b0` | 52 | 28 KB | `mech3/zrdrvalve.cpp` |
| `0x4e4d20-0x4eada0` | 161 | 112 KB | (between mech3/zrdrvalve.cpp and gamezmech3/zclass/class.c) |
| `0x4eaea0-0x4ed8b0` | 50 | 53 KB | `gamezmech3/zclass/class.c` |
| `0x4ed8f0-0x4edab0` | 11 | 7 KB | (between gamezmech3/zclass/class.c and gamezmech3/zclass/cls_di.c) |
| `0x4eddb0-0x4f0f90` | 22 | 66 KB | `gamezmech3/zclass/cls_di.c` |
| `0x4f12e0-0x4f1c60` | 8 | 13 KB | (between gamezmech3/zclass/cls_di.c and gamezmech3/zclass/cls_world.c) |
| `0x4f1cd0-0x4f3040` | 36 | 29 KB | `gamezmech3/zclass/cls_world.c` |
| `0x4f3170-0x4f32a0` | 2 | 0 KB | (between gamezmech3/zclass/cls_world.c and gamezmech3/zclass/camera.c) |
| `0x4f32e0-0x4f6c80` | 48 | 79 KB | `gamezmech3/zclass/camera.c` |
| `0x4f6d40-0x4f8220` | 31 | 27 KB | (between gamezmech3/zclass/camera.c and gamezmech3/zclass/list.c) |
| `0x4f8390-0x4f84b0` | 2 | 7 KB | `gamezmech3/zclass/list.c` |
| `0x4f89a0-0x4f90e0` | 17 | 8 KB | (between gamezmech3/zclass/list.c and gamezmech3/zclass/cls_util.c) |
| `0x4f9190-0x4f9b50` | 12 | 12 KB | `gamezmech3/zclass/cls_util.c` |
| `0x4f9bc0-0x4f9e50` | 5 | 3 KB | (between gamezmech3/zclass/cls_util.c and gamezmech3/zclass/window.c) |
| `0x4f9e90-0x4fa3e0` | 10 | 9 KB | `gamezmech3/zclass/window.c` |
| `0x4fa4c0-0x4fa680` | 5 | 3 KB | `gamezmech3/zclass/display.c` |
| `0x4fa720-0x4fb510` | 20 | 23 KB | `gamezmech3/zclass/light.c` |
| `0x4fb600-0x4fb600` | 1 | 0 KB | (between gamezmech3/zclass/light.c and gamezmech3/zclass/object3d.c) |
| `0x4fb650-0x4fc3f0` | 20 | 20 KB | `gamezmech3/zclass/object3d.c` |
| `0x4fc470-0x4fd1c0` | 20 | 18 KB | (between gamezmech3/zclass/object3d.c and gamezmech3/zclass/switch.c) |
| `0x4fd1e0-0x4fd230` | 2 | 1 KB | `gamezmech3/zclass/switch.c` |
| `0x4fd280-0x4fd680` | 8 | 7 KB | `gamezmech3/zclass/sound.c` |
| `0x4fd780-0x4fd780` | 1 | 1 KB | (between gamezmech3/zclass/sound.c and gamezmech3/zclass/animate.c) |
| `0x4fd890-0x4fd950` | 4 | 2 KB | `gamezmech3/zclass/animate.c` |
| `0x4fda20-0x4fdab0` | 2 | 3 KB | (between gamezmech3/zclass/animate.c and gamezmech3/zclass/seq.c) |
| `0x4fdc70-0x4fdf50` | 8 | 7 KB | `gamezmech3/zclass/seq.c` |
| `0x4fe030-0x4fe1d0` | 8 | 2 KB | (between gamezmech3/zclass/seq.c and gamezmech3/zclass/cls_zbd.c) |
| `0x4fe210-0x4ff7a0` | 11 | 29 KB | `gamezmech3/zclass/cls_zbd.c` |
| `0x4ff8f0-0x4ff920` | 2 | 0 KB | (between gamezmech3/zclass/cls_zbd.c and gamezmech3/zdeclient/zdec_crater.cpp) |
| `0x4ff940-0x4ff940` | 1 | 1 KB | `gamezmech3/zdeclient/zdec_crater.cpp` |
| `0x4ffaa0-0x501a80` | 23 | 46 KB | (between gamezmech3/zdeclient/zdec_crater.cpp and gamezmech3/zdeclient/zdec_init.cpp) |
| `0x501ab0-0x501ab0` | 1 | 4 KB | `gamezmech3/zdeclient/zdec_init.cpp` |
| `0x501f90-0x5020f0` | 8 | 2 KB | (between gamezmech3/zdeclient/zdec_init.cpp and gamezmech3/zdeclient/zdec_qsand.cpp) |
| `0x502110-0x502110` | 1 | 1 KB | `gamezmech3/zdeclient/zdec_qsand.cpp` |
| `0x502230-0x507970` | 65 | 112 KB | (between gamezmech3/zdeclient/zdec_qsand.cpp and gamezmech3/zeffect/zeff_anim_run.c) |
| `0x5079b0-0x508170` | 5 | 8 KB | `gamezmech3/zeffect/zeff_anim_run.c` |
| `0x508310-0x5090e0` | 24 | 18 KB | (between gamezmech3/zeffect/zeff_anim_run.c and gamezmech3/zeffect/zeff_anim_init.c) |
| `0x5090f0-0x50ae60` | 17 | 48 KB | `gamezmech3/zeffect/zeff_anim_init.c` |
| `0x50b0e0-0x50b6f0` | 14 | 8 KB | (between gamezmech3/zeffect/zeff_anim_init.c and gamezmech3/zeffect/zeff_anim_save.c) |
| `0x50b700-0x50be30` | 4 | 10 KB | `gamezmech3/zeffect/zeff_anim_save.c` |
| `0x50ca20-0x50d2b0` | 16 | 10 KB | (between gamezmech3/zeffect/zeff_anim_save.c and gamezmech3/zeffect/zeff_init.c) |
| `0x50d2c0-0x50d2c0` | 1 | 2 KB | `gamezmech3/zeffect/zeff_init.c` |
| `0x50d570-0x50da50` | 9 | 5 KB | (between gamezmech3/zeffect/zeff_init.c and gamezmech3/zerror/zerr_old.c) |
| `0x50da70-0x50da70` | 1 | 0 KB | `gamezmech3/zerror/zerr_old.c` |
| `0x50da90-0x50de00` | 8 | 3 KB | (between gamezmech3/zerror/zerr_old.c and gamezmech3/zgeometry/zgeo_model.cpp) |
| `0x50dea0-0x50f7b0` | 24 | 31 KB | `gamezmech3/zgeometry/zgeo_model.cpp` |
| `0x50f950-0x50f950` | 1 | 0 KB | (between gamezmech3/zgeometry/zgeo_model.cpp and gamezmech3/zgeometry/zgeo_weiler.cpp) |
| `0x50f960-0x5154d0` | 45 | 107 KB | `gamezmech3/zgeometry/zgeo_weiler.cpp` |
| `0x5158c0-0x516290` | 12 | 11 KB | (between gamezmech3/zgeometry/zgeo_weiler.cpp and gamezmech3/zgeometry/zgeo_convexify.cpp) |
| `0x5162d0-0x5162d0` | 1 | 5 KB | `gamezmech3/zgeometry/zgeo_convexify.cpp` |
| `0x5166c0-0x517250` | 11 | 15 KB | (between gamezmech3/zgeometry/zgeo_convexify.cpp and gamezmech3/zimage/zimg_winfonts.cpp) |
| `0x517280-0x517280` | 1 | 0 KB | `gamezmech3/zimage/zimg_winfonts.cpp` |
| `0x5172c0-0x5179d0` | 18 | 11 KB | (between gamezmech3/zimage/zimg_winfonts.cpp and gamezmech3/zimage/zimg_library.cpp) |
| `0x517b10-0x517b10` | 1 | 1 KB | `gamezmech3/zimage/zimg_library.cpp` |
| `0x517be0-0x518080` | 20 | 6 KB | (between gamezmech3/zimage/zimg_library.cpp and gamezmech3/zimage/zimg_texture.cpp) |
| `0x5180a0-0x518160` | 2 | 2 KB | `gamezmech3/zimage/zimg_texture.cpp` |
| `0x518210-0x519cc0` | 50 | 40 KB | (between gamezmech3/zimage/zimg_texture.cpp and gamezmech3/zimage/zimg_fonts.cpp) |
| `0x519df0-0x519e50` | 2 | 1 KB | `gamezmech3/zimage/zimg_fonts.cpp` |
| `0x519f70-0x51a0d0` | 4 | 2 KB | (between gamezmech3/zimage/zimg_fonts.cpp and gamezmech3/zinput/zin_kbd.cpp) |
| `0x51a170-0x51aa00` | 10 | 11 KB | `gamezmech3/zinput/zin_kbd.cpp` |
| `0x51ab90-0x51b020` | 6 | 4 KB | (between gamezmech3/zinput/zin_kbd.cpp and gamezmech3/zinput/zin_init.cpp) |
| `0x51b0a0-0x51b0a0` | 1 | 0 KB | `gamezmech3/zinput/zin_init.cpp` |
| `0x51b170-0x526920` | 222 | 184 KB | (between gamezmech3/zinput/zin_init.cpp and gamezmech3/zmodel/gmod_init.c) |
| `0x5269a0-0x526ae0` | 12 | 2 KB | `gamezmech3/zmodel/gmod_init.c` |
| `0x526b80-0x536070` | 46 | 296 KB | (between gamezmech3/zmodel/gmod_init.c and gamezmech3/zmodel/gmod_matl.c) |
| `0x5360c0-0x5377e0` | 19 | 30 KB | `gamezmech3/zmodel/gmod_matl.c` |
| `0x537830-0x5380c0` | 11 | 12 KB | (between gamezmech3/zmodel/gmod_matl.c and gamezmech3/zmodel/gmod_const.c) |
| `0x5380e0-0x53c6c0` | 45 | 103 KB | `gamezmech3/zmodel/gmod_const.c` |
| `0x53cc40-0x542710` | 28 | 130 KB | (between gamezmech3/zmodel/gmod_const.c and gamezmech3/zmodel/gmod_cookie.cpp) |
| `0x542730-0x542730` | 1 | 5 KB | `gamezmech3/zmodel/gmod_cookie.cpp` |
| `0x542bb0-0x543ac0` | 25 | 21 KB | (between gamezmech3/zmodel/gmod_cookie.cpp and gamezmech3/zmodel/gmod_light.c) |
| `0x543b60-0x543b60` | 1 | 2 KB | `gamezmech3/zmodel/gmod_light.c` |
| `0x543d90-0x5490e0` | 18 | 122 KB | (between gamezmech3/zmodel/gmod_light.c and gamezmech3/zmodel/gmod_puff.cpp) |
| `0x549410-0x549410` | 1 | 0 KB | `gamezmech3/zmodel/gmod_puff.cpp` |
| `0x5495a0-0x557150` | 171 | 273 KB | (between gamezmech3/zmodel/gmod_puff.cpp and gamezmech3/zreader/zreader.cpp) |
| `0x557180-0x557180` | 1 | 1 KB | `gamezmech3/zreader/zreader.cpp` |
| `0x5572c0-0x566250` | 82 | 233 KB | (between gamezmech3/zreader/zreader.cpp and gamezmech3/zrender/zrndr_draw.c) |
| `0x5662d0-0x566890` | 4 | 12 KB | `gamezmech3/zrender/zrndr_draw.c` |
| `0x566c90-0x56f260` | 90 | 142 KB | (between gamezmech3/zrender/zrndr_draw.c and gamezmech3/zsound/zsnd_parm.cpp) |
| `0x56f280-0x56f280` | 1 | 1 KB | `gamezmech3/zsound/zsnd_parm.cpp` |
| `0x56f370-0x570280` | 32 | 19 KB | (between gamezmech3/zsound/zsnd_parm.cpp and gamezmech3/zsound/zsnd_init.cpp) |
| `0x5702a0-0x571460` | 9 | 24 KB | `gamezmech3/zsound/zsnd_init.cpp` |
| `0x5715c0-0x5723d0` | 21 | 17 KB | (between gamezmech3/zsound/zsnd_init.cpp and gamezmech3/zsound/zsnd_cd.cpp) |
| `0x572450-0x5731f0` | 19 | 17 KB | `gamezmech3/zsound/zsnd_cd.cpp` |
| `0x573270-0x573420` | 6 | 3 KB | (between gamezmech3/zsound/zsnd_cd.cpp and gamezmech3/zsound/zsnd_grp.cpp) |
| `0x573560-0x5739f0` | 2 | 16 KB | `gamezmech3/zsound/zsnd_grp.cpp` |
| `0x573cc0-0x574fd0` | 31 | 25 KB | (between gamezmech3/zsound/zsnd_grp.cpp and gamezmech3/zsound/zsnd_create.cpp) |
| `0x574ff0-0x575600` | 5 | 8 KB | `gamezmech3/zsound/zsnd_create.cpp` |
| `0x575690-0x576520` | 34 | 21 KB | (between gamezmech3/zsound/zsnd_create.cpp and gamezmech3/zutil/zutl_zar.cpp) |
| `0x5765b0-0x5765b0` | 1 | 0 KB | `gamezmech3/zutil/zutl_zar.cpp` |
| `0x576650-0x576f70` | 39 | 12 KB | (between gamezmech3/zutil/zutl_zar.cpp and gamezmech3/zvideo/zvid_buff.c) |
| `0x576f90-0x576f90` | 1 | 1 KB | `gamezmech3/zvideo/zvid_buff.c` |
| `0x5770f0-0x578f60` | 24 | 19 KB | `gamezmech3/zvideo/zvid_dd.c` |
| `0x579000-0x579bd0` | 31 | 14 KB | (between gamezmech3/zvideo/zvid_dd.c and gamezmech3/zvideo/zvid_init.c) |
| `0x579c60-0x579c60` | 1 | 2 KB | `gamezmech3/zvideo/zvid_init.c` |
| `0x579ea0-0x57a3d0` | 8 | 8 KB | (between gamezmech3/zvideo/zvid_init.c and gamezmech3/zvideo/zvid_ddd3d.c) |
| `0x57a740-0x580350` | 17 | 28 KB | `gamezmech3/zvideo/zvid_ddd3d.c` |
| `0x580550-0x583870` | 49 | 62 KB | (between gamezmech3/zvideo/zvid_ddd3d.c and gamezmech3/zweapon/zwep_init.c) |
| `0x583880-0x583880` | 1 | 18 KB | `gamezmech3/zweapon/zwep_init.c` |
| `0x5847c0-0x587450` | 59 | 53 KB | (between gamezmech3/zweapon/zwep_init.c and gamezmech3/znetwork/znet_session.cpp) |
| `0x587480-0x587580` | 2 | 3 KB | `gamezmech3/znetwork/znet_session.cpp` |
| `0x587680-0x588130` | 21 | 14 KB | (between gamezmech3/znetwork/znet_session.cpp and gamezmech3/znetwork/znet_dplay.cpp) |
| `0x588180-0x589540` | 18 | 17 KB | `gamezmech3/znetwork/znet_dplay.cpp` |
| `0x5895b0-0x58fcd0` | 137 | 113 KB | (between gamezmech3/znetwork/znet_dplay.cpp and gamezmech3/zgame/zgameapp.cpp) |
| `0x58fd70-0x58fd70` | 1 | 4 KB | `gamezmech3/zgame/zgameapp.cpp` |
| `0x5900f9-0x591290` | 24 | 25 KB | (between gamezmech3/zgame/zgameapp.cpp and gamezmech3/zinterp/zinterp_parse.cpp) |
| `0x591300-0x591300` | 1 | 61 KB | `gamezmech3/zinterp/zinterp_parse.cpp` |
| `0x594dc0-0x598d30` | 108 | 97 KB | (between gamezmech3/zinterp/zinterp_parse.cpp and gamezmech3/zfmv/fmv_script.cpp) |
| `0x598d80-0x598d80` | 1 | 15 KB | `gamezmech3/zfmv/fmv_script.cpp` |
| `0x599570-0x59a530` | 35 | 18 KB | (between gamezmech3/zfmv/fmv_script.cpp and gamezmech3/zfmv/fmv_stream.cpp) |
| `0x59a650-0x59ae50` | 6 | 14 KB | `gamezmech3/zfmv/fmv_stream.cpp` |
| `0x59b040-0x59b250` | 5 | 3 KB | (between gamezmech3/zfmv/fmv_stream.cpp and gamezmech3/zfmv/fmv_main.cpp) |
| `0x59b280-0x59b280` | 1 | 1 KB | `gamezmech3/zfmv/fmv_main.cpp` |
| `0x59b2f0-0x59f080` | 65 | 73 KB | (between gamezmech3/zfmv/fmv_main.cpp and gamezmech3/ztwod/twod_dialog.cpp) |
| `0x59f810-0x59f8d0` | 3 | 2 KB | `gamezmech3/ztwod/twod_dialog.cpp` |
| `0x59fa10-0x5a01f0` | 27 | 10 KB | (between gamezmech3/ztwod/twod_dialog.cpp and gamezmech3/ztwod/twod_wintext.cpp) |
| `0x5a0200-0x5a0710` | 6 | 5 KB | `gamezmech3/ztwod/twod_wintext.cpp` |
| `0x5a0d20-0x5a80c0` | 195 | 116 KB | (between gamezmech3/ztwod/twod_wintext.cpp and gamezmech3/zentity/zent_init.cpp) |
| `0x5a80d0-0x5a8390` | 7 | 3 KB | `gamezmech3/zentity/zent_init.cpp` |
| `0x5a84c0-0x5a9970` | 51 | 28 KB | (between gamezmech3/zentity/zent_init.cpp and gamezmech3/zsiscript/si_script.cpp) |
| `0x5a9990-0x5aa3b0` | 10 | 11 KB | `gamezmech3/zsiscript/si_script.cpp` |
| `0x5aa450-0x5baf80` | 2903 | 397 KB | (after gamezmech3/zsiscript/si_script.cpp) |
