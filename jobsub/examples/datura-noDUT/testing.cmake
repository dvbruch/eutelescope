#
# This file defines a number of data-driven tests based on the example configuration
# in this directory. If you have access the the files and paths defined below you
# can run the tests by running 'make test' in the build directory in the EUTelescope root
#

# ======================================================================
# ======================================================================
# TestJobsubExampleDaturaNoDUT: based on config in jobsub/examples/datura-noDUT
# ======================================================================
# ======================================================================

    SET( testdir "${PROJECT_BINARY_DIR}/Testing/test_datura-noDUT" )
    SET( jobsubdir "$ENV{EUTELESCOPE}/jobsub" )
    SET( exampledir "${jobsubdir}/examples/datura-noDUT" )

    # the run number
    SET( RunNr "97" )
    # run number padded with leading zeros
    execute_process(COMMAND sh -c "printf %06d ${RunNr}" OUTPUT_VARIABLE PaddedRunNr)
 
   # only needed in the last step to test the results of EUTel against a set of reference files:
    SET( stattestdir "$ENV{EUTELESCOPE}/test/stattest/bin" )
    SET( referencedatadir "/afs/desy.de/group/telescopes/EutelTestData/TestExampleDaturaNoDUT" )

    SET( executable python -tt ${jobsubdir}/jobsub.py )
    # options: use config, use csv, change native path to central AFS location, reduce number of events to 200k
    SET( jobsubOptions --config=${exampledir}/config.cfg -csv ${exampledir}/runlist.csv -o NativePath=${referencedatadir} -o MaxRecordNumber=200000)


    # all this regular expressions must be matched for the tests to pass.
    # the order of the expressions must be matched in the test execution!
    # additional statements can be defined for each test individually
    SET( jobsub_pass_regex_1 "Now running Marlin" )
    SET( marlin_pass_regex_1 "Processing event.*in run ${PaddedRunNr}" )
    SET( jobsub_pass_regex_2 "Marlin execution done" )

    SET( generic_fail_regex "ERROR" "CRITICAL" "segmentation violation" "There were [0-9]* error messages reported")


#
#  STEP 0: PREPARE TEST DIRECTORY
#
	ADD_TEST( TestJobsubExampleDaturaNoDUTCleanup sh -c "[ -d ${testdir} ] && rm -rf ${testdir} || echo 'no cleanup needed.'" )
	ADD_TEST( TestJobsubExampleDaturaNoDUTSetup sh -c "mkdir -p ${testdir}/output/histograms  && mkdir -p ${testdir}/output/database && mkdir -p ${testdir}/output/logs && mkdir -p ${testdir}/output/lcio" )
#
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#  STEP 1: CONVERTER
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#

    ADD_TEST( NAME TestJobsubExampleDaturaNoDUTConverterRun 
              WORKING_DIRECTORY "${testdir}"
	      COMMAND ${executable} ${jobsubOptions} converter ${RunNr} )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTConverterRun PROPERTIES
        # test will pass if ALL of the following expressions are matched
        PASS_REGULAR_EXPRESSION "${jobsub_pass_regex_1}.*${marlin_pass_regex_1}.*${jobsub_pass_regex_2}"
        # test will fail if ANY of the following expressions is matched 
        FAIL_REGULAR_EXPRESSION "${generic_fail_regex}"
	# test depends on earlier steps
	DEPENDS TestJobsubExampleDaturaNoDUTSetup
    )


    # now check if the expected output files exist and look ok
    ADD_TEST( TestJobsubExampleDaturaNoDUTConverterLog sh -c "[ -f ${testdir}/output/logs/converter-${PaddedRunNr}.zip ]" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTConverterLog PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTConverterRun)

    ADD_TEST( TestJobsubExampleDaturaNoDUTConverterHisto sh -c "[ -f ${testdir}/output/histograms/run${PaddedRunNr}-converter.root ]" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTConverterHisto PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTConverterRun)

    ADD_TEST( TestJobsubExampleDaturaNoDUTConverterHotpix sh -c "[ -f ${testdir}/output/database/run${PaddedRunNr}-hotpixel.slcio ] && lcio_check_col_elements --expelements 6  hotpixel_m26  ${testdir}/output/database/run${PaddedRunNr}-hotpixel.slcio" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTConverterHotpix PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTConverterRun)

    ADD_TEST( TestJobsubExampleDaturaNoDUTConverterOutput sh -c "[ -f ${testdir}/output/lcio/run${PaddedRunNr}-converter.slcio ] && lcio_check_col_elements --expelements 6 zsdata_m26 ${testdir}/output/lcio/run${PaddedRunNr}-converter.slcio" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTConverterOutput PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTConverterRun)




#
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#  STEP 2: CLUSTERING
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#
    ADD_TEST( NAME TestJobsubExampleDaturaNoDUTClusteringRun 
              WORKING_DIRECTORY ${testdir} 
	      COMMAND ${executable} ${jobsubOptions} clustering ${RunNr} )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTClusteringRun PROPERTIES
        # test will pass if ALL of the following expressions are matched
        PASS_REGULAR_EXPRESSION "${jobsub_pass_regex_1}.*${marlin_pass_regex_1}.*${jobsub_pass_regex_2}"
        # test will fail if ANY of the following expressions is matched 
        FAIL_REGULAR_EXPRESSION "${generic_fail_regex}"
	# test depends on earlier steps
	DEPENDS TestJobsubExampleDaturaNoDUTConverterOutput
	)
    # now check if the expected output files exist and look ok
    ADD_TEST( TestJobsubExampleDaturaNoDUTClusteringLog sh -c "[ -f ${testdir}/output/logs/clustering-${PaddedRunNr}.zip ]" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTClusteringLog PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTClusteringRun)

    ADD_TEST( TestJobsubExampleDaturaNoDUTClusteringHisto sh -c "[ -f ${testdir}/output/histograms/run${PaddedRunNr}-clustering.root ]" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTClusteringHisto PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTClusteringRun)

    # we expect an average of 24.4 clusters per event
    ADD_TEST( TestJobsubExampleDaturaNoDUTClusteringOutput sh -c "[ -f ${testdir}/output/lcio/run${PaddedRunNr}-clustering.slcio ] && lcio_check_col_elements --average --expelements 24 cluster_m26 ${testdir}/output/lcio/run${PaddedRunNr}-clustering.slcio" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTClusteringOutput PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTClusteringRun)



#
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#  STEP 3: HITMAKER
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#
    ADD_TEST( NAME TestJobsubExampleDaturaNoDUTHitmakerRun 
              WORKING_DIRECTORY ${testdir} 
	      COMMAND ${executable} ${jobsubOptions} hitmaker ${RunNr} )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTHitmakerRun PROPERTIES
        # test will pass if ALL of the following expressions are matched
        PASS_REGULAR_EXPRESSION "${jobsub_pass_regex_1}.*${marlin_pass_regex_1}.*${jobsub_pass_regex_2}"
        # test will fail if ANY of the following expressions is matched 
        FAIL_REGULAR_EXPRESSION "${generic_fail_regex}"
	# test depends on earlier steps
	DEPENDS TestJobsubExampleDaturaNoDUTClusteringOutput
	)
    # now check if the expected output files exist and look ok
    ADD_TEST( TestJobsubExampleDaturaNoDUTHitmakerLog sh -c "[ -f ${testdir}/output/logs/hitmaker-${PaddedRunNr}.zip ]" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTHitmakerLog PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTHitmakerRun)

    ADD_TEST( TestJobsubExampleDaturaNoDUTHitmakerHisto sh -c "[ -f ${testdir}/output/histograms/run${PaddedRunNr}-hitmaker.root ]" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTHitmakerHisto PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTHitmakerRun)

    ADD_TEST( TestJobsubExampleDaturaNoDUTHitmakerPrealign sh -c "[ -f ${testdir}/output/database/run${PaddedRunNr}-prealignment.slcio ] && lcio_check_col_elements --expelements 6  alignment  ${testdir}/output/database/run${PaddedRunNr}-prealignment.slcio" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTHitmakerPrealign PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTHitmakerRun)

    # we expect an average hit number of 24 for run 97 (wide geometry) using the example configuration
    ADD_TEST( TestJobsubExampleDaturaNoDUTHitmakerOutput sh -c "[ -f ${testdir}/output/lcio/run${PaddedRunNr}-hitmaker.slcio ] && lcio_check_col_elements -a --expelements 24 hit ${testdir}/output/lcio/run${PaddedRunNr}-hitmaker.slcio" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTHitmakerOutput PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTHitmakerRun)


#
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#  STEP 4: ALIGNMENT
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#
    # all this regular expressions must be matched for the test to pass
    SET( align_pass_regex_1 "Initialising Mille" )
    SET( align_pass_regex_2 "Pede successfully finished" )

    ADD_TEST( NAME TestJobsubExampleDaturaNoDUTAlignRun 
              WORKING_DIRECTORY ${testdir} 
	      COMMAND ${executable} ${jobsubOptions} align ${RunNr} )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTAlignRun PROPERTIES
        # test will pass if ALL of the following expressions are matched
        PASS_REGULAR_EXPRESSION "${jobsub_pass_regex_1}.*${align_pass_regex_1}.*${marlin_pass_regex_1}.*${align_pass_regex_2}.*${jobsub_pass_regex_2}"
        # test will fail if ANY of the following expressions is matched 
        FAIL_REGULAR_EXPRESSION "${generic_fail_regex}"
	# test depends on earlier steps
	DEPENDS TestJobsubExampleDaturaNoDUTHitmakerOutput
	)
    # now check if the expected output files exist and look ok
    ADD_TEST( TestJobsubExampleDaturaNoDUTAlignLog sh -c "[ -f ${testdir}/output/logs/align-${PaddedRunNr}.zip ]" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTAlignLog PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTAlignRun)

    ADD_TEST( TestJobsubExampleDaturaNoDUTAlignHisto sh -c "[ -f ${testdir}/output/histograms/run${PaddedRunNr}-alignment.root ]" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTAlignHisto PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTAlignRun)

    ADD_TEST( TestJobsubExampleDaturaNoDUTAlignDB sh -c "[ -f ${testdir}/output/database/run${PaddedRunNr}-alignment.slcio ] && lcio_check_col_elements --expelements 6  alignment  ${testdir}/output/database/run${PaddedRunNr}-alignment.slcio" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTAlignDB PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTAlignRun)

    ADD_TEST( TestJobsubExampleDaturaNoDUTAlignOutput sh -c "[ -f ${testdir}/output/database/run${PaddedRunNr}-align-mille.bin -a -f ${testdir}/output/database/run${PaddedRunNr}-pede-steer.txt ] " )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTAlignOutput PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTAlignRun)





#
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#  STEP 5: FITTER
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#
    #SET( fit_pass_regex_1 "Total number of reconstructed tracks *[0-9][0-9][0-9][0-9][0-9]+" )

    ADD_TEST( NAME TestJobsubExampleDaturaNoDUTFitterRun 
              WORKING_DIRECTORY ${testdir} 
	      COMMAND ${executable} ${jobsubOptions} fitter ${RunNr} )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTFitterRun PROPERTIES
        # test will pass if ALL of the following expressions are matched
        PASS_REGULAR_EXPRESSION "${jobsub_pass_regex_1}.*${marlin_pass_regex_1}.*${jobsub_pass_regex_2}"
        # test will fail if ANY of the following expressions is matched 
        FAIL_REGULAR_EXPRESSION "${generic_fail_regex}"
	# test depends on earlier steps
	DEPENDS TestJobsubExampleDaturaNoDUTAlignRun
	# fitter step sometimes takes a bit longer (in s)
	TIMEOUT 2000
	)
    # now check if the expected output files exist and look ok
    ADD_TEST( TestJobsubExampleDaturaNoDUTFitterLog sh -c "[ -f ${testdir}/output/logs/fitter-${PaddedRunNr}.zip ]" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTFitterLog PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTFitterRun)

    ADD_TEST( TestJobsubExampleDaturaNoDUTFitterHisto sh -c "[ -f ${testdir}/output/histograms/run${PaddedRunNr}-fitter.root ]" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTFitterHisto PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTFitterRun)

    # we expect to see between 1 and 3 tracks in every event 
    # but tolerate if this is not the case in 40% of the events (empty events are counted)
    ADD_TEST( TestJobsubExampleDaturaNoDUTFitterOutput sh -c "[ -f ${testdir}/output/lcio/run${PaddedRunNr}-track.slcio ] && lcio_check_col_elements --pedantic --expelements 2 --abselementerror 1 --releventerror .40 track0 ${testdir}/output/lcio/run${PaddedRunNr}-track.slcio" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTFitterOutput PROPERTIES DEPENDS TestJobsubExampleDaturaNoDUTFitterRun)

#
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#  STEP 6: StatTest
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#
    SET( executable "python ${stattestdir}/runtests.py" )

    # all this regular expressions must be matched for the test to pass
    SET( fit_pass_regex_1 "SUCCESS" )
    SET( fit_fail_regex "FAILED" "NOT PASSED" "Error" "segmentation violation")

    # run stattest tool on output from previous step and test it against reference file; test are configured in specified config file (*.qa)

    ADD_TEST( TestJobsubExampleDaturaNoDUTStatTestClustering sh -c "PYTHONPATH=$ROOTSYS/lib:$PYTHONPATH ${executable} --cdash -g ${testdir}/output/stattest_report_clustering.pdf ${referencedatadir}/StatTestConf_DaturaNoDUTClustering.qa ${testdir}/output/histograms/run${PaddedRunNr}-clustering.root ${referencedatadir}/run${PaddedRunNr}-clustering.root" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTStatTestClustering PROPERTIES
        # test will pass if ALL of the following expressions are matched
        PASS_REGULAR_EXPRESSION "${fit_pass_regex_1}"
        # test will fail if ANY of the following expressions is matched 
        FAIL_REGULAR_EXPRESSION "${fit_fail_regex}"
	# test depends on earlier steps
	DEPENDS TestJobsubExampleDaturaNoDUTClusteringRun
	)


    ADD_TEST( TestJobsubExampleDaturaNoDUTStatTestAlign sh -c "PYTHONPATH=$ROOTSYS/lib:$PYTHONPATH ${executable} --cdash -g ${testdir}/output/stattest_report_align.pdf ${referencedatadir}/StatTestConf_DaturaNoDUTAlign.qa ${testdir}/output/histograms/run${PaddedRunNr}-alignment.root ${referencedatadir}/run${PaddedRunNr}-alignment.root" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTStatTestAlign PROPERTIES
        # test will pass if ALL of the following expressions are matched
        PASS_REGULAR_EXPRESSION "${fit_pass_regex_1}"
        # test will fail if ANY of the following expressions is matched 
        FAIL_REGULAR_EXPRESSION "${fit_fail_regex}"
	# test depends on earlier steps
	DEPENDS TestJobsubExampleDaturaNoDUTAlignRun
	)


    ADD_TEST( TestJobsubExampleDaturaNoDUTStatTestFitter sh -c "PYTHONPATH=$ROOTSYS/lib:$PYTHONPATH ${executable} --cdash  -g${testdir}/output/stattest_report_fitter.pdf ${referencedatadir}/StatTestConf_DaturaNoDUTFitter.qa ${testdir}/output/histograms/run${PaddedRunNr}-fitter.root ${referencedatadir}/run${PaddedRunNr}-tracking.root" )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTStatTestFitter PROPERTIES
        # test will pass if ALL of the following expressions are matched
        PASS_REGULAR_EXPRESSION "${fit_pass_regex_1}"
        # test will fail if ANY of the following expressions is matched 
        FAIL_REGULAR_EXPRESSION "${fit_fail_regex}"
	# test depends on earlier steps
	DEPENDS TestJobsubExampleDaturaNoDUTFitterRun
	)


#
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#  STEP 7: MemChecks
# +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#
  # STEP 1-5 VARIANTS USED FOR MEMCHECKS ONLY:
    SET( executable python -tt ${jobsubdir}/jobsub.py )
    # options for memcheck runs: reduced run range, plain output for valgrind parsing
    SET( jobsubMemCheckOptions --config=${exampledir}/config.cfg -csv ${exampledir}/runlist.csv -o NativePath=${referencedatadir} -o MaxRecordNumber=2000 --plain)

  # Converter run with reduced run range
    ADD_TEST( NAME TestJobsubExampleDaturaNoDUTConverterRunMemCheck
              WORKING_DIRECTORY "${testdir}"
	      COMMAND ${executable} ${jobsubMemCheckOptions} converter ${RunNr} )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTConverterRunMemCheck PROPERTIES
        PASS_REGULAR_EXPRESSION "${jobsub_pass_regex_1}.*${marlin_pass_regex_1}.*${jobsub_pass_regex_2}"
        FAIL_REGULAR_EXPRESSION "${generic_fail_regex}"
    )

    ADD_TEST( NAME TestJobsubExampleDaturaNoDUTClusteringRunMemCheck
              WORKING_DIRECTORY ${testdir} 
	      COMMAND ${executable} ${jobsubMemCheckOptions} clustering ${RunNr} )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTClusteringRunMemCheck PROPERTIES
        PASS_REGULAR_EXPRESSION "${jobsub_pass_regex_1}.*${marlin_pass_regex_1}.*${jobsub_pass_regex_2}"
        FAIL_REGULAR_EXPRESSION "${generic_fail_regex}"
	)

    ADD_TEST( NAME TestJobsubExampleDaturaNoDUTHitmakerRunMemCheck
              WORKING_DIRECTORY ${testdir} 
	      COMMAND ${executable} ${jobsubMemCheckOptions} hitmaker ${RunNr} )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTHitmakerRunMemCheck PROPERTIES
        PASS_REGULAR_EXPRESSION "${jobsub_pass_regex_1}.*${marlin_pass_regex_1}.*${jobsub_pass_regex_2}"
        FAIL_REGULAR_EXPRESSION "${generic_fail_regex}"
	)

    ADD_TEST( NAME TestJobsubExampleDaturaNoDUTAlignRunMemCheck
              WORKING_DIRECTORY ${testdir} 
	      COMMAND ${executable} ${jobsubMemCheckOptions} align ${RunNr} )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTAlignRunMemCheck PROPERTIES
        PASS_REGULAR_EXPRESSION "${jobsub_pass_regex_1}.*${marlin_pass_regex_1}.*${jobsub_pass_regex_2}"
        FAIL_REGULAR_EXPRESSION "${generic_fail_regex}"
	)

    ADD_TEST( NAME TestJobsubExampleDaturaNoDUTFitterRunMemCheck
              WORKING_DIRECTORY ${testdir} 
	      COMMAND ${executable} ${jobsubMemCheckOptions} fitter ${RunNr} )
    SET_TESTS_PROPERTIES (TestJobsubExampleDaturaNoDUTFitterRunMemCheck PROPERTIES
        PASS_REGULAR_EXPRESSION "${jobsub_pass_regex_1}.*${marlin_pass_regex_1}.*${jobsub_pass_regex_2}"
        FAIL_REGULAR_EXPRESSION "${generic_fail_regex}"
	)
