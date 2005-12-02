
VPATH=../../

CC=gcc
STRIP=strip

CFLAGS=-O2 -Wall -fno-strict-aliasing $(OSCFLAGS) $(CPUCFLAGS)
STRIPFLAGS=--strip-unneeded --remove-section=.comment

TARGETSYSTEM?=$(shell echo `uname -m`-`uname`)

OS=$(shell echo $(TARGETSYSTEM) | cut -d '-' -f 2)
CPU=$(shell echo $(TARGETSYSTEM) | cut -d '-' -f 1)

# OS specific settings

ifeq ($(OS), MorphOS)
	OSCFLAGS=-noixemul
endif

ifeq ($(OS), Linux)
	OSOBJS= \
		sys_linux.o \
		cd_linux.o \
		snd_linux.o

	OSSWOBJS=vid_x11.o
	OSSWLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext
endif

# CPU specific settings

ifeq ($(CPU), ppc)
	CPUCFLAGS=-DBIGENDIAN
endif

OBJS= \
	host.o \
	snd_main.o \
	snd_mem.o \
	snd_mix.o \
	cl_input.o \
	keys.o \
	net_chan.o \
	net_udp.o \
	pr_exec.o \
	pr_edict.o \
	pr_cmds.o \
	pmove.o \
	pmovetst.o \
	sv_ccmds.o \
	sv_save.o \
	sv_ents.o \
	sv_init.o \
	sv_main.o \
	sv_move.o \
	sv_nchan.o \
	sv_phys.o \
	sv_send.o \
	sv_user.o \
	sv_world.o \
	r_aclip.o \
	r_alias.o \
	r_bsp.o \
	r_draw.o \
	r_edge.o \
	r_efrag.o \
	r_light.o \
	r_main.o \
	r_model.o \
	r_misc.o \
	r_part.o \
	r_sky.o \
	r_sprite.o \
	r_surf.o \
	r_rast.o \
	r_vars.o \
	d_edge.o \
	d_fill.o \
	d_init.o \
	d_modech.o \
	d_polyse.o \
	d_scan.o \
	d_sky.o \
	d_sprite.o \
	d_surf.o \
	d_vars.o \
	d_zpoint.o \
	cl_auth.o \
	cl_cam.o \
	cl_capture.o \
	cl_cmd.o \
	cl_demo.o \
	cl_ents.o \
	cl_fchecks.o \
	cl_fragmsgs.o \
	cl_ignore.o \
	cl_logging.o \
	cl_main.o \
	cl_parse.o \
	cl_pred.o \
	cl_slist.o \
	cl_tent.o \
	cl_view.o \
	cmd.o \
	common.o \
	com_msg.o \
	console.o \
	crc.o \
	cvar.o \
	image.o \
	mathlib.o \
	mdfour.o \
	menu.o \
	cl_sbar.o \
	cl_screen.o \
	skin.o \
	teamplay.o \
	version.o \
	wad.o \
	zone.o \
	modules.o \
	match_tools.o \
	utils.o \
	rulesets.o \
	config_manager.o \
	mp3_player.o \
	fmod.o \
	vid.o \
	$(OSOBJS)

SWOBJS=$(OSSWOBJS)
all:
	@echo $(TARGETSYSTEM)
	@echo OS: $(OS)
	@echo CPU: $(CPU)

	mkdir -p objects/$(TARGETSYSTEM)
	(cd objects/$(TARGETSYSTEM); make -f ../../Makefile release)

clean:
	rm -rf objects

release: fodquake-sw

fodquake-sw: $(OBJS) $(SWOBJS)
	$(CC) $(CFLAGS) $^ -lm $(OSSWLDFLAGS) -lpng -o $@.db
	$(STRIP) $(STRIPFLAGS) $@.db -o $@
	
