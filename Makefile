CC = gcc
LDFLAGS = -lm #-lSaturn
CFLAGS = -g -O3 -ftree-vectorize #-Wall  #-finstrument-functions
pmodules = utilities.o image_lib.o draw_lib.o image_filters.o level_set.o contour_lib.o\
					 water_shed.o
cmodules = common.o tiff_io.o tiff_image.o aip.o eval.o seq.o trace.o distance.o dag.o\
					 trajectory.o match.o bar.o image_adapt.o error.o adjust_scan_bias.o\
					 seed.o whisker_io.o whisker_io_whisker1.o whisker_io_whiskbin1.o
modules = $(pmodules) $(cmodules)
TESTS = test_whisker_io evaltest aiptest
APPS  = whisk whisker_convert
LIBS  = libwhisk.so

all: checkos $(APPS) $(LIBS) python #$(TESTS)

rebuild: clean all

.PHONY: checkos
checkos:
ifndef OS
CP = cp
OS =
else
CP = copy
RM = del 
endif

all: $(APPS)

tests: $(TESTS)

whisk: whisk.c $(modules) $(modules:.o=.h)

whisker_io_main.o: whisker_io.c
	$(CC) -c $(LDFLAGS) $(CFLAGS) -DWHISKER_IO_CONVERTER $< -o $@

whisker_convert: whisker_io_main.o $(filter-out whisker_io.o,$(modules)) $(modules:.o=.h)
	$(CC) $(LDFLAGS) $(CFLAGS) $+ -o $@

libwhisk.so: $(modules)
ifneq ($(OS), Windows_NT)
	@echo
	@echo    ~~~ Building $@
	@echo
	libtool -dynamic -o $@ $+ -lc $(LDFLAGS)
else
	@echo
	@echo    ~~~ Windows detected.  Building $(@:.so=.dll)
	@echo
	$(CC) -shared $(CFLAGS) -o $(@:.so=.dll) $+ $(LDFLAGS)  
endif

python: $(LIBS) trace.py
ifneq ($(OS), Windows_NT)
	$(CP) $+ ./ui/whiskerdata
	$(CP) $+ ./ui
	make -C ./ui/genetiff
else
	$(CP) libwhisk.dll ui\whiskerdata 
	$(CP) trace.py ui\whiskerdata
	$(CP) libwhisk.dll ui
	$(CP) trace.py ui
	make -C ui\genetiff
endif

awk: $(pmodules:.o=.toawk)

%.toawk: %.p
	awk -f manager.awk $< > $*.c

%.o: %.p
	awk -f manager.awk $< > $*.c
	$(CC) $(CFLAGS) -c $*.c

$(cmodules): $(cmodules:.o=.h)

test_whisker_io: test_whisker_io.c $(modules) 

seedtest: seedtest.c $(modules)

stripetest: stripetest.c $(modules)

evaltest: evaltest.c $(modules)
# 	$(CC) $(LDFLAGS) $(CFLAGS) -DEVAL_TEST_1 $+ -o $@
# 	-./$@
# 	$(CC) $(LDFLAGS) $(CFLAGS) -DEVAL_TEST_2 $+ -o $@
# 	-./$@
	$(CC) $(LDFLAGS) $(CFLAGS) -DEVAL_TEST_3 $+ -o $@
	-./$@
##$(CC) $(LDFLAGS) $(CFLAGS) -DEVAL_TEST_4 $+ -o $@
##-./$@
##$(CC) $(LDFLAGS) $(CFLAGS) -DEVAL_TEST_5 $+ -o $@
##-./$@

aiptest: aip.c 
	$(CC) $(LDFLAGS) $(CFLAGS) -DTEST_AIP -DTEST_AIP_1 $< -o $@
	./$@
	$(CC) $(LDFLAGS) $(CFLAGS) -DTEST_AIP -DTEST_AIP_2 $< -o $@
	./$@
	$(CC) $(LDFLAGS) $(CFLAGS) -DTEST_AIP -DTEST_AIP_3 $< -o $@
	./$@
	$(CC) $(LDFLAGS) $(CFLAGS) -DTEST_AIP -DTEST_AIP_4 $< -o $@
	./$@
	$(CC) $(LDFLAGS) $(CFLAGS) -DTEST_AIP -DTEST_AIP_5 $< -o $@
	./$@
	$(CC) $(LDFLAGS) $(CFLAGS) -DTEST_AIP -DTEST_AIP_6 $< -o $@
	./$@
	$(CC) $(LDFLAGS) $(CFLAGS) -DTEST_AIP -DTEST_AIP_7 $< -o $@
	./$@
	$(CC) $(LDFLAGS) $(CFLAGS) -DTEST_AIP -DTEST_AIP_8 $< -o $@
	./$@

.PHONY: clean
clean: checkos
	-$(RM) *.o $(pmodules:.o=.c)
	-$(RM) *.so
	-$(RM) ./ui/whiskerdata/*.so
	-$(RM) ./ui/*.so
	-$(RM) ./ui/whiskerdata/trace.py
	-$(RM) ./ui/trace.py
	-$(RM) *.dll
	-$(RM) *.exe
	-$(RM) *.pyc
	-$(RM) -r *.dSYM
	-$(RM) $(LIBS)
	-$(RM) $(TESTS)
	-$(RM) $(APPS)

.PHONY: superclean
superclean: clean
	-$(RM) *.tif
	-$(RM) gmon.out
	-$(RM) SaturnErrors.txt
	-$(RM) *.whiskers
	-$(RM) *.trajectories
	-$(RM) *.bar
	-$(RM) *.detectorbank
