#
# This is the XCSoar build script.  To compile XCSoar, you must
# specify the target platform, e.g. for Pocket PC 2003, type:
#
#   make TARGET=PPC2003
#
# The following parameters may be specified on the "make" command
# line:
#
#   TARGET      The name of the target platform.  See the TARGETS variable
#               below for a list of valid target platforms.
#
#   ENABLE_SDL  If set to "y", the UI is drawn with libSDL.
#
#   DEBUG       If set to "y", the debugging version of XCSoar is built
#               (default is "y")
#
#   WERROR      Make all compiler warnings fatal (default is $DEBUG)
#
#   V           Verbosity; 1 is the default, and prints terse information.
#               0 means quiet, and 2 prints the full compiler commands.
#
#   FIXED       "y" means use fixed point math (for FPU-less platforms)
#
#   LTO         "y" enables gcc's link-time optimization flag (experimental,
#               requires gcc 4.5)
#
#   CLANG       "y" to use clang instead of gcc
#

.DEFAULT_GOAL := all

topdir = .

include $(topdir)/build/bool.mk
include $(topdir)/build/common.mk
include $(topdir)/build/targets.mk
include $(topdir)/build/debug.mk
include $(topdir)/build/coverage.mk
include $(topdir)/build/options.mk
include $(topdir)/build/boost.mk
include $(topdir)/build/sdl.mk
include $(topdir)/build/gconf.mk
include $(topdir)/build/flags.mk
include $(topdir)/build/charset.mk
include $(topdir)/build/warnings.mk
include $(topdir)/build/compile.mk
include $(topdir)/build/llvm.mk
include $(topdir)/build/tools.mk
include $(topdir)/build/resource.mk
include $(topdir)/build/version.mk
include $(topdir)/build/generate.mk
include $(topdir)/build/doco.mk

# Create libraries for zzip, jasper and compatibility stuff
include $(topdir)/build/libutil.mk
include $(topdir)/build/libmath.mk
include $(topdir)/build/libprofile.mk
include $(topdir)/build/zlib.mk
include $(topdir)/build/zzip.mk
include $(topdir)/build/jasper.mk
include $(topdir)/build/driver.mk
include $(topdir)/build/io.mk
include $(topdir)/build/shapelib.mk
include $(topdir)/build/task.mk
include $(topdir)/build/datafield.mk
include $(topdir)/build/screen.mk
include $(topdir)/build/form.mk
include $(topdir)/build/harness.mk

include $(topdir)/build/setup.mk
include $(topdir)/build/launch.mk
include $(topdir)/build/vali.mk
include $(topdir)/build/test.mk

######## output files

ifeq ($(HAVE_POSIX),y)
PROGRAM_NAME = xcsoar
else
PROGRAM_NAME = XCSoar
endif

OUTPUTS := $(TARGET_BIN_DIR)/$(PROGRAM_NAME)$(TARGET_EXEEXT) $(VALI_XCS)
OUTPUTS += $(XCSOARSETUP_DLL) $(XCSOARLAUNCH_DLL)

include $(topdir)/build/dist.mk
include $(topdir)/build/install.mk

######## compiler flags

INCLUDES += -I$(SRC) -I$(ENGINE_SRC_DIR) -I$(SRC)/WayPoint
CPPFLAGS += $(GCONF_CPPFLAGS)

####### linker configuration

LDFLAGS = $(TARGET_LDFLAGS) $(FLAGS_PROFILE)
LDLIBS = $(TARGET_LDLIBS) $(GCONF_LDLIBS)

####### sources

DIALOG_SOURCES = \
	$(SRC)/Dialogs/XML.cpp \
	$(SRC)/Dialogs/Message.cpp \
	$(SRC)/Dialogs/ListPicker.cpp \
	$(SRC)/Dialogs/dlgAirspace.cpp \
	$(SRC)/Dialogs/dlgAirspaceColours.cpp \
	$(SRC)/Dialogs/dlgAirspacePatterns.cpp \
	$(SRC)/Dialogs/dlgAirspaceDetails.cpp \
	$(SRC)/Dialogs/dlgAirspaceSelect.cpp \
	$(SRC)/Dialogs/dlgAirspaceWarning.cpp \
	$(SRC)/Dialogs/dlgAnalysis.cpp \
	$(SRC)/Dialogs/dlgBasicSettings.cpp \
	$(SRC)/Dialogs/dlgBrightness.cpp \
	$(SRC)/Dialogs/dlgChecklist.cpp \
	$(SRC)/Dialogs/dlgComboPicker.cpp \
	$(SRC)/Dialogs/dlgConfiguration.cpp \
	$(SRC)/Dialogs/dlgConfigPage.cpp \
	$(SRC)/Dialogs/dlgConfigFonts.cpp \
	$(SRC)/Dialogs/dlgConfigInfoboxes.cpp \
	$(SRC)/Dialogs/dlgConfigWaypoints.cpp \
	$(SRC)/Dialogs/dlgConfigurationVario.cpp \
	$(SRC)/Dialogs/dlgFlarmTraffic.cpp \
	$(SRC)/Dialogs/dlgFlarmTrafficDetails.cpp \
	$(SRC)/Dialogs/dlgHelp.cpp \
	$(SRC)/Dialogs/dlgLoggerReplay.cpp \
	$(SRC)/Dialogs/dlgSimulatorPrompt.cpp \
	$(SRC)/Dialogs/dlgStartup.cpp \
	$(SRC)/Dialogs/dlgStatus.cpp \
	$(SRC)/Dialogs/dlgSwitches.cpp \
	\
	$(SRC)/Dialogs/dlgTaskManager.cpp \
	$(SRC)/Dialogs/dlgTaskEdit.cpp \
	$(SRC)/Dialogs/dlgTaskProperties.cpp \
	$(SRC)/Dialogs/dlgTaskList.cpp \
	$(SRC)/Dialogs/dlgTaskType.cpp \
	$(SRC)/Dialogs/dlgTaskPoint.cpp \
	$(SRC)/Dialogs/dlgTaskPointType.cpp \
	$(SRC)/Dialogs/dlgTaskHelpers.cpp \
	$(SRC)/Dialogs/dlgTaskCalculator.cpp \
	$(SRC)/Dialogs/dlgTarget.cpp \
	\
	$(SRC)/Dialogs/dlgTeamCode.cpp \
	$(SRC)/Dialogs/dlgTextEntry.cpp \
	$(SRC)/Dialogs/dlgTextEntry_Keyboard.cpp \
	$(SRC)/Dialogs/dlgThermalAssistant.cpp \
	$(SRC)/Dialogs/dlgVegaDemo.cpp \
	$(SRC)/Dialogs/dlgVoice.cpp \
	$(SRC)/Dialogs/dlgWeather.cpp \
	$(SRC)/Dialogs/dlgWayPointDetails.cpp \
	$(SRC)/Dialogs/dlgWaypointEdit.cpp \
	$(SRC)/Dialogs/dlgWayPointSelect.cpp \
	$(SRC)/Dialogs/dlgWindSettings.cpp \
	$(SRC)/Dialogs/dlgFontEdit.cpp \


XCSOAR_SOURCES := \
	$(IO_SRC_DIR)/ConfiguredFile.cpp \
	$(IO_SRC_DIR)/DataFile.cpp \
	$(SRC)/Airspace/ProtectedAirspaceWarningManager.cpp \
	$(SRC)/Task/ProtectedTaskManager.cpp \
	$(SRC)/TaskStore.cpp \
	\
	$(SRC)/Poco/RWLock.cpp \
	\
	$(SRC)/Airspace/AirspaceGlue.cpp \
	$(SRC)/Airspace/AirspaceParser.cpp \
	$(SRC)/Airspace/AirspaceVisibility.cpp \
	\
	$(SRC)/Atmosphere.cpp \
	$(SRC)/ClimbAverageCalculator.cpp \
	$(SRC)/ConditionMonitor.cpp \
	$(SRC)/DateTime.cpp \
	$(SRC)/FLARM/FlarmId.cpp \
	$(SRC)/FLARM/State.cpp \
	$(SRC)/FLARM/FLARMNet.cpp \
	$(SRC)/FLARM/Traffic.cpp \
	$(SRC)/FLARM/FlarmCalculations.cpp \
	$(SRC)/GlideComputer.cpp \
	$(SRC)/GlideComputerBlackboard.cpp \
	$(SRC)/GlideComputerAirData.cpp \
	$(SRC)/GlideComputerInterface.cpp \
	$(SRC)/GlideComputerStats.cpp \
	$(SRC)/GlideComputerTask.cpp \
	$(SRC)/GlideRatio.cpp \
	$(SRC)/Terrain/GlideTerrain.cpp \
	$(SRC)/Logger/Logger.cpp \
	$(SRC)/Logger/LoggerFRecord.cpp \
	$(SRC)/Logger/LoggerGRecord.cpp \
	$(SRC)/Logger/LoggerEPE.cpp \
	$(SRC)/Logger/LoggerImpl.cpp \
	$(SRC)/Logger/IGCWriter.cpp \
	$(SRC)/Logger/MD5.cpp \
	$(SRC)/Logger/NMEALogger.cpp \
	$(SRC)/Logger/ExternalLogger.cpp \
	$(SRC)/NMEA/ThermalBand.cpp \
	$(SRC)/NMEA/InputLine.cpp \
	$(SRC)/Replay/Replay.cpp \
	$(SRC)/Replay/IgcReplay.cpp \
	$(SRC)/Replay/IgcReplayGlue.cpp \
	$(SRC)/Replay/NmeaReplay.cpp \
	$(SRC)/Replay/NmeaReplayGlue.cpp \
	$(SRC)/TeamCodeCalculation.cpp \
	$(SRC)/ThermalLocator.cpp \
	$(SRC)/ThermalBase.cpp \
	$(SRC)/WayPoint/WayPointGlue.cpp \
	$(SRC)/WayPoint/WayPointFile.cpp \
	$(SRC)/WayPoint/WayPointFileWinPilot.cpp \
	$(SRC)/WayPoint/WayPointFileSeeYou.cpp \
	$(SRC)/WayPoint/WayPointFileZander.cpp \
	$(SRC)/WayPoint/WayPointRenderer.cpp \
	$(SRC)/Wind/WindAnalyser.cpp \
	$(SRC)/Wind/WindMeasurementList.cpp \
	$(SRC)/Wind/WindStore.cpp \
	$(SRC)/Wind/WindZigZag.cpp \
	\
	$(SRC)/Gauge/ThermalAssistantWindow.cpp \
	$(SRC)/Gauge/FlarmTrafficWindow.cpp \
	$(SRC)/Gauge/GaugeCDI.cpp \
	$(SRC)/Gauge/GaugeFLARM.cpp \
	$(SRC)/Gauge/GaugeThermalAssistant.cpp \
	$(SRC)/Gauge/GaugeVario.cpp \
	$(SRC)/Gauge/TaskView.cpp \
	\
	$(SRC)/AirfieldDetails.cpp \
	$(SRC)/MenuData.cpp \
	$(SRC)/MenuBar.cpp \
	$(SRC)/ButtonLabel.cpp \
	$(SRC)/Dialogs.cpp \
	$(SRC)/ExpandMacros.cpp \
	$(SRC)/InfoBoxes/Content/Factory.cpp \
	$(SRC)/InfoBoxes/Content/Alternate.cpp \
	$(SRC)/InfoBoxes/Content/Altitude.cpp \
	$(SRC)/InfoBoxes/Content/Direction.cpp \
  $(SRC)/InfoBoxes/Content/Glide.cpp \
	$(SRC)/InfoBoxes/Content/Other.cpp \
	$(SRC)/InfoBoxes/Content/Speed.cpp \
	$(SRC)/InfoBoxes/Content/Task.cpp \
	$(SRC)/InfoBoxes/Content/Team.cpp \
	$(SRC)/InfoBoxes/Content/Thermal.cpp \
	$(SRC)/InfoBoxes/Content/Time.cpp \
	$(SRC)/InfoBoxes/Content/Weather.cpp \
	$(SRC)/InfoBoxes/InfoBoxWindow.cpp \
	$(SRC)/InfoBoxes/InfoBoxLayout.cpp \
	$(SRC)/InfoBoxes/InfoBoxManager.cpp \
	$(SRC)/InputEvents.cpp \
	$(SRC)/InputEventsActions.cpp \
	$(SRC)/Pages.cpp \
	$(SRC)/StatusMessage.cpp \
	$(SRC)/PopupMessage.cpp \
	$(SRC)/Message.cpp \
	$(SRC)/LogFile.cpp \
	\
	$(SRC)/MapCanvas.cpp \
	$(SRC)/MapDrawHelper.cpp \
	$(SRC)/BackgroundDrawHelper.cpp \
	$(SRC)/Projection.cpp \
	$(SRC)/RenderObservationZone.cpp \
	$(SRC)/RenderTaskPoint.cpp \
	$(SRC)/RenderTask.cpp \
	$(SRC)/ChartProjection.cpp \
	$(SRC)/MapWindow.cpp \
	$(SRC)/MapWindowAirspace.cpp \
	$(SRC)/MapWindowEvents.cpp \
	$(SRC)/MapWindowGlideRange.cpp \
	$(SRC)/MapWindowLabels.cpp \
	$(SRC)/MapWindowProjection.cpp \
	$(SRC)/MapWindowRender.cpp \
	$(SRC)/MapWindowScale.cpp \
	$(SRC)/MapWindowSymbols.cpp \
	$(SRC)/MapWindowTask.cpp \
	$(SRC)/MapWindowTarget.cpp \
	$(SRC)/MapWindowThermal.cpp \
	$(SRC)/MapWindowTimer.cpp \
	$(SRC)/MapWindowTraffic.cpp \
	$(SRC)/MapWindowTrail.cpp \
	$(SRC)/MapWindowWaypoints.cpp \
	$(SRC)/GlueMapWindow.cpp \
	$(SRC)/GlueMapWindowAirspace.cpp \
	$(SRC)/GlueMapWindowEvents.cpp \
	$(SRC)/GestureManager.cpp \
	$(SRC)/DrawThread.cpp \
	\
	$(SRC)/DeviceBlackboard.cpp \
	$(SRC)/InstrumentBlackboard.cpp \
	$(SRC)/InterfaceBlackboard.cpp \
	$(SRC)/MapProjectionBlackboard.cpp \
	$(SRC)/MapWindowBlackboard.cpp \
	$(SRC)/SettingsMapBlackboard.cpp \
	$(SRC)/SettingsComputerBlackboard.cpp \
	$(SRC)/CalculationThread.cpp \
	$(SRC)/InstrumentThread.cpp \
	\
	$(SRC)/Topology/TopologyFile.cpp \
	$(SRC)/Topology/TopologyStore.cpp \
	$(SRC)/Topology/TopologyGlue.cpp \
	$(SRC)/Topology/XShape.cpp \
	$(SRC)/Terrain/RasterBuffer.cpp \
	$(SRC)/Terrain/RasterProjection.cpp \
	$(SRC)/Terrain/RasterMap.cpp \
	$(SRC)/Terrain/RasterTile.cpp \
	$(SRC)/Terrain/RasterTerrain.cpp \
	$(SRC)/Terrain/RasterWeather.cpp \
	$(SRC)/Terrain/HeightMatrix.cpp \
	$(SRC)/Terrain/RasterRenderer.cpp \
	$(SRC)/Terrain/TerrainRenderer.cpp \
	$(SRC)/Terrain/WeatherTerrainRenderer.cpp \
	$(SRC)/Marks.cpp \
	\
	$(SRC)/Persist.cpp \
	$(SRC)/FlightStatistics.cpp \
	\
	$(SRC)/Simulator.cpp \
	$(SRC)/Asset.cpp \
	$(SRC)/Appearance.cpp \
	$(SRC)/Hardware/Battery.cpp \
	$(SRC)/Hardware/Display.cpp \
	$(SRC)/MOFile.cpp \
	$(SRC)/Language.cpp \
	$(SRC)/LocalPath.cpp \
	$(SRC)/Interface.cpp \
	$(SRC)/ProgressGlue.cpp \
	$(SRC)/LocalTime.cpp \
	$(SRC)/Units.cpp \
	$(SRC)/FLARM/FlarmDetails.cpp \
	$(SRC)/UtilsFile.cpp \
	$(SRC)/UtilsFont.cpp \
	$(SRC)/UtilsSettings.cpp \
	$(SRC)/UtilsSystem.cpp \
	$(SRC)/UtilsText.cpp \
	$(SRC)/CommandLine.cpp \
	$(SRC)/OS/FileUtil.cpp \
	$(SRC)/OS/FileMapping.cpp \
	$(SRC)/OS/PathName.cpp \
	$(SRC)/Version.cpp \
	$(SRC)/Audio/Sound.cpp \
	$(SRC)/Audio/VegaVoice.cpp \
	$(SRC)/Compatibility/fmode.c \
	$(SRC)/Compatibility/string.c 	\
	$(SRC)/Profile/Profile.cpp \
	$(SRC)/Profile/Writer.cpp \
	$(SRC)/Profile/ProfileGlue.cpp \
	$(SRC)/Profile/ProfileKeys.cpp \
	$(SRC)/xmlParser.cpp \
	$(SRC)/Thread/Thread.cpp \
	$(SRC)/Thread/StoppableThread.cpp \
	$(SRC)/Thread/WorkerThread.cpp \
	$(SRC)/Thread/Mutex.cpp \
	$(SRC)/Thread/Debug.cpp \
	\
	$(SRC)/Math/Screen.cpp \
	$(SRC)/Math/SunEphemeris.cpp \
	\
	$(SRC)/Screen/Blank.cpp \
	$(SRC)/Screen/Chart.cpp \
	$(SRC)/Screen/Fonts.cpp \
	$(SRC)/Screen/Layout.cpp \
	$(SRC)/Screen/UnitSymbol.cpp \
	$(SRC)/Screen/Graphics.cpp \
	$(SRC)/Screen/TextInBox.cpp \
	$(SRC)/Screen/Ramp.cpp \
	$(SRC)/Screen/LabelBlock.cpp \
	$(SRC)/Screen/ProgressWindow.cpp \
	$(SRC)/ResourceLoader.cpp \
	\
	$(SRC)/Polar/Polar.cpp \
	$(SRC)/Polar/Loader.cpp \
	$(SRC)/Polar/WinPilot.cpp \
	$(SRC)/Polar/BuiltIn.cpp \
	$(SRC)/Polar/Historical.cpp \
	\
	$(SRC)/Blackboard.cpp \
	$(SRC)/Protection.cpp \
	$(SRC)/ProcessTimer.cpp \
	$(SRC)/MainWindow.cpp \
	$(SRC)/Components.cpp \
	$(SRC)/XCSoar.cpp \
	\
	$(SRC)/Device/Driver.cpp \
	$(SRC)/Device/Declaration.cpp \
	$(SRC)/Device/Register.cpp \
	$(SRC)/Device/List.cpp \
	$(SRC)/Device/device.cpp \
	$(SRC)/Device/Descriptor.cpp \
	$(SRC)/Device/All.cpp \
	$(SRC)/Device/Geoid.cpp \
	$(SRC)/Device/Parser.cpp \
	$(SRC)/Device/Port.cpp \
	$(SRC)/Device/NullPort.cpp \
	$(SRC)/Device/SerialPort.cpp \
	$(SRC)/Device/FLARM.cpp \
	$(SRC)/Device/Internal.cpp \
	$(DIALOG_SOURCES)

#	$(SRC)/VarioSound.cpp \
#	$(SRC)/WaveThread.cpp \


ifneq ($(findstring $(TARGET),ALTAIR ALTAIRPORTRAIT),)
XCSOAR_SOURCES += $(SRC)/Hardware/AltairControl.cpp
endif

XCSOAR_OBJS = $(call SRC_TO_OBJ,$(XCSOAR_SOURCES))
XCSOAR_LDADD = \
	$(PROFILE_LIBS) \
	$(IO_LIBS) \
	$(DATA_FIELD_LIBS) \
	$(FORM_LIBS) \
	$(SCREEN_LIBS) \
	$(DRIVER_LIBS) \
	$(ENGINE_LIBS) \
	$(SHAPELIB_LIBS) \
	$(JASPER_LIBS) \
	$(ZZIP_LIBS) \
	$(UTIL_LIBS) \
	$(MATH_LIBS) \
	$(RESOURCE_BINARY)

include $(topdir)/build/gettext.mk
include $(topdir)/build/cab.mk

all: all-$(TARGET)

# if no TARGET is set, build all targets
all-: $(addprefix call-,$(DEFAULT_TARGETS))
call-%:
	$(MAKE) TARGET=$(patsubst call-%,%,$@) DEBUG=$(DEBUG) V=$(V)

$(addprefix all-,$(TARGETS)): all-%: $(OUTPUTS)

EVERYTHING = $(OUTPUTS) debug-$(TARGET) build-check
ifeq ($(TARGET),UNIX)
EVERYTHING += $(TESTS)
endif

everything-: $(addprefix call-everything-,$(DEFAULT_TARGETS))
call-everything-%:
	$(MAKE) everything TARGET=$(patsubst call-everything-%,%,$@) EVERYTHING=$(EVERYTHING) V=$(V)

$(addprefix everything-,$(TARGETS)): everything-%: $(EVERYTHING)

everything: everything-$(TARGET)

####### products

ifeq ($(HAVE_CE),y)

SYNCE_PCP = synce-pcp

install: XCSoar.exe
	@echo Copying to device...
	$(SYNCE_PCP) -f XCSoar.exe ':/Program Files/XCSoar/XCSoar.exe'

endif

ifneq ($(NOSTRIP_SUFFIX),)
$(TARGET_BIN_DIR)/$(PROGRAM_NAME)$(TARGET_EXEEXT): $(TARGET_BIN_DIR)/$(PROGRAM_NAME)$(NOSTRIP_SUFFIX)$(TARGET_EXEEXT)
	@$(NQ)echo "  STRIP   $@"
	$(Q)$(STRIP) $< -o $@
	$(Q)$(SIZE) $@
endif

$(TARGET_BIN_DIR)/$(PROGRAM_NAME)$(NOSTRIP_SUFFIX)$(TARGET_EXEEXT): CPPFLAGS += $(SCREEN_CPPFLAGS)
$(TARGET_BIN_DIR)/$(PROGRAM_NAME)$(NOSTRIP_SUFFIX)$(TARGET_EXEEXT): $(XCSOAR_OBJS) $(XCSOAR_LDADD) | $(TARGET_BIN_DIR)/dirstamp
	@$(NQ)echo "  LINK    $@"
	$(Q)$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) $(SCREEN_LDLIBS) -o $@

IGNORE	:= \( -name .svn -o -name CVS -o -name .git \) -prune -o

clean: cleancov FORCE
	@$(NQ)echo "cleaning all"
	$(Q)rm -rf output
	$(RM) $(BUILDTESTS)

cleancov: FORCE
	@$(NQ)echo "cleaning cov"
	$(Q)find ./ $(IGNORE) \( \
		   -name '*.bb' \
		-o -name '*.bbg' \
		-o -name '*.gcda' \
		-o -name '*.gcda.info' \
		-o -name '*.gcno' \
		-o -name '*.gcno.info' \
	\) -type f -print | xargs -r $(RM)

.PHONY: FORCE

ifneq ($(wildcard $(TARGET_OUTPUT_DIR)/src/*.d),)
include $(wildcard $(TARGET_OUTPUT_DIR)/src/*.d)
endif
ifneq ($(wildcard $(TARGET_OUTPUT_DIR)/src/*/*.d),)
include $(wildcard $(TARGET_OUTPUT_DIR)/src/*/*.d)
endif
ifneq ($(wildcard $(TARGET_OUTPUT_DIR)/src/*/*/*.d),)
include $(wildcard $(TARGET_OUTPUT_DIR)/src/*/*/*.d)
endif

