# tested on Devuan/excalibur and Debian/trixie

THE_APP   := FarosPLC
XDEB_ARCH := armhf
XTRIPLET  := arm-linux-gnueabihf
XGCC_ABI  := arm-linux-generic-elf-32bit
XGDB_ABIS := $(XGCC_ABI)

$(info ---------> QtCreator $(THE_APP) cross-toolchain for ($(XDEB_ARCH), $(XTRIPLET)))

# -----------------------------------------------------------------------------
.PHONY: all clean distclean

all: \
	toolchain \
	sdktool
	@echo ---------\> done $@

clean: \
	clean_toolchain \
	clean_sdktool
	@echo ---------\> done $@

distclean: \
	clean \
	distclean_toolchain \
	#
	@echo ---------\> done $@

# -----------------------------------------------------------------------------
.PHONY: toolchain clean_toolchain distclean_toolchain

DEB_PACKAGES:= \
	crossbuild-essential-$(XDEB_ARCH) \
	gdb-multiarch \
	\
	libc6-$(XDEB_ARCH)-cross \
	libc6-dev:$(XDEB_ARCH) \
	libmodbus-dev \
	libmodbus-dev:$(XDEB_ARCH) \
	libgpiod-dev \
	libgpiod-dev:$(XDEB_ARCH) \
	\
	qtcreator \
	openssh-client \
	rsync \
	#

toolchain:
	sudo dpkg --add-architecture $(XDEB_ARCH)
	sudo aptitude update
	# it's normal: "All rc.d operations denied by policy" 
	# it's normal: asking for "Setting up tzdata" ... 
	# it's normal: "Exec format error" for glib-compile-schemas, gio-querymodules, gdk-pixbuf-query-loaders, gtk-query-immodules-3.0, gdk-pixbuf-query-loaders, ...
	sudo aptitude install $(DEB_PACKAGES)
	sudo aptitude clean
	@echo ---------\> done $@

clean_toolchain:
	@echo "if you really want: sudo aptitude remove $(DEB_PACKAGES)"
	@echo ---------\> done $@

distclean_toolchain:
	@echo "if you really want: sudo aptitude purge $(DEB_PACKAGES)"
	@echo ---------\> done $@

# -----------------------------------------------------------------------------
.PHONY: sdktool clean_sdktool

sdktool: clean_sdktool
	# https://github.com/qt-creator/qt-creator/blob/16.0/src/tools/sdktool/README.md
	sudo /usr/libexec/qtcreator/sdktool addTC \
		--id   "ProjectExplorer.ToolChain.Gcc:$(THE_APP).$(XDEB_ARCH).gcc" \
		--name                               "$(THE_APP).$(XDEB_ARCH).gcc" \
		--language C \
		--path     /usr/bin/$(XTRIPLET)-gcc \
		--abi      $(XGCC_ABI)
	sudo /usr/libexec/qtcreator/sdktool addTC \
		--id   "ProjectExplorer.ToolChain.Gcc:$(THE_APP).$(XDEB_ARCH).g++" \
		--name                               "$(THE_APP).$(XDEB_ARCH).g++" \
		--language Cxx \
		--path     /usr/bin/$(XTRIPLET)-g++ \
		--abi      $(XGCC_ABI)
	sudo /usr/libexec/qtcreator/sdktool addDebugger \
		--id   "$(THE_APP).$(XDEB_ARCH).gdb" \
		--name "$(THE_APP).$(XDEB_ARCH).gdb" \
		--engine 1 \
		--binary /usr/bin/gdb-multiarch \
		--abis   $(XGDB_ABIS)
	sudo /usr/libexec/qtcreator/sdktool addKit \
		--id   "$(THE_APP).$(XDEB_ARCH).kit" \
		--name "$(THE_APP).$(XDEB_ARCH).kit" \
		--debuggerid   "$(THE_APP).$(XDEB_ARCH).gdb" \
		--devicetype   "GenericLinuxOsType" \
		--Ctoolchain   "ProjectExplorer.ToolChain.Gcc:$(THE_APP).$(XDEB_ARCH).gcc" \
		--Cxxtoolchain "ProjectExplorer.ToolChain.Gcc:$(THE_APP).$(XDEB_ARCH).g++" \
		# --device "{33118fec-67d9-4ed0-b36b-de610f4bed53}"
	@echo ---------\> done $@

clean_sdktool:
	@echo ---------\> /usr/share/qtcreator/QtProject/qtcreator/{toolchains,qtversion,profiles}.xml
	-sudo /usr/libexec/qtcreator/sdktool rmTC       --id "ProjectExplorer.ToolChain.Gcc:$(THE_APP).$(XDEB_ARCH).gcc"
	-sudo /usr/libexec/qtcreator/sdktool rmTC       --id "ProjectExplorer.ToolChain.Gcc:$(THE_APP).$(XDEB_ARCH).g++"
	-sudo /usr/libexec/qtcreator/sdktool rmDebugger --id "$(THE_APP).$(XDEB_ARCH).gdb"
	-sudo /usr/libexec/qtcreator/sdktool rmKit      --id "$(THE_APP).$(XDEB_ARCH).kit"


