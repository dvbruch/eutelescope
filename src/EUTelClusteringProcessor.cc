// -*- mode: c++; mode: auto-fill; mode: flyspell-prog; -*-
// Author Antonio Bulgheroni, INFN <mailto:antonio.bulgheroni@gmail.com>
// Version $Id: EUTelClusteringProcessor.cc,v 1.34 2007-09-18 13:03:50 bulgheroni Exp $
/*
 *   This source code is part of the Eutelescope package of Marlin.
 *   You are free to use this source files for your own development as
 *   long as it stays in a public research context. You are not
 *   allowed to use it for commercial purpose. You must put this
 *   header with author names in all development based on this file.
 *
 */

// eutelescope includes ".h" 
#include "EUTELESCOPE.h"
#include "EUTelExceptions.h"
#include "EUTelRunHeaderImpl.h"
#include "EUTelEventImpl.h"
#include "EUTelClusteringProcessor.h"
#include "EUTelVirtualCluster.h"
#include "EUTelFFClusterImpl.h"
#include "EUTelExceptions.h"
#include "EUTelHistogramManager.h"
#include "EUTelMatrixDecoder.h"
#include "EUTelSparseDataImpl.h"
#include "EUTelSparseClusterImpl.h"
#include "EUTelSparseData2Impl.h"
#include "EUTelSparseCluster2Impl.h"

// marlin includes ".h"
#include "marlin/Processor.h"
#include "marlin/AIDAProcessor.h"
#include "marlin/Exceptions.h"

// lcio includes <.h> 
#include <UTIL/CellIDEncoder.h>
#include <IMPL/TrackerRawDataImpl.h>
#include <IMPL/TrackerDataImpl.h>
#include <IMPL/TrackerPulseImpl.h>
#include <IMPL/LCCollectionVec.h>

#ifdef MARLIN_USE_AIDA
// aida includes <.h>
#include <AIDA/IHistogramFactory.h>
#include <AIDA/IHistogram1D.h>
#include <AIDA/IHistogram2D.h>
#include <AIDA/ITree.h>
#endif

// system includes <>
#ifdef MARLINDEBUG
#include <fstream> 
#include <cassert>
#endif
#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <list>



using namespace std;
using namespace lcio;
using namespace marlin;
using namespace eutelescope;

#ifdef MARLINDEBUG
/// /* DEBUG */ ofstream logfile;
#endif

// definition of static members mainly used to name histograms
#ifdef MARLIN_USE_AIDA
std::string EUTelClusteringProcessor::_clusterSignalHistoName      = "clusterSignal";
std::string EUTelClusteringProcessor::_seedSignalHistoName         = "seedSignal";
std::string EUTelClusteringProcessor::_hitMapHistoName             = "hitMap";
std::string EUTelClusteringProcessor::_seedSNRHistoName            = "seedSNR";
std::string EUTelClusteringProcessor::_clusterNoiseHistoName       = "clusterNoise";
std::string EUTelClusteringProcessor::_clusterSNRHistoName         = "clusterSNR";
std::string EUTelClusteringProcessor::_eventMultiplicityHistoName  = "eventMultiplicity";
#endif

EUTelClusteringProcessor::EUTelClusteringProcessor () : Processor("EUTelClusteringProcessor") {

  // modify processor description
  _description =
    "EUTelClusteringProcessor is looking for clusters into a calibrated pixel matrix.";

  // first of all we need to register the input collection
  registerInputCollection (LCIO::TRACKERDATA, "NZSDataCollectionName",
			   "Input calibrated data not zero suppressed collection name",
			   _nzsDataCollectionName, string ("data"));

  registerInputCollection (LCIO::TRACKERDATA, "ZSDataCollectionName",
			   "Input of Zero Suppressed data",
			   _zsDataCollectionName, string ("zsdata") );

  registerInputCollection (LCIO::TRACKERDATA, "NoiseCollectionName",
			   "Noise (input) collection name",
			   _noiseCollectionName, string("noise"));

  registerInputCollection (LCIO::TRACKERRAWDATA, "StatusCollectionName",
			   "Pixel status (input) collection name",
			   _statusCollectionName, string("status"));

  registerOutputCollection(LCIO::TRACKERPULSE, "PulseCollectionName",
			   "Cluster (output) collection name",
			   _pulseCollectionName, string("cluster"));

  // I believe it is safer not allowing the dummyCollection to be
  // renamed by the user. I prefer to set it once for ever here and
  // eventually, only if really needed, in the future allow add
  // another registerOutputCollection.
  _dummyCollectionName = "original_data";


  // now the optional parameters
  registerProcessorParameter ("ClusteringAlgo",
			      "Select here which algorithm should be used for clustering.\n"
			      "Available algorithms are:\n"
			      "-> FixedFrame: for custer with a given size",
			      _nzsClusteringAlgo, string(EUTELESCOPE::FIXEDFRAME));

  registerProcessorParameter ("ZSClusteringAlgo",
			      "Select here which algorithm should be used for clustering.\n"
			      "Available algorithms are:\n"
			      "-> SparseCluster: for cluster in ZS frame\n"
			      "-> SparseCluster2: for cluster in ZS frame with better performance"
			      "-> FixedFrame: for cluster with a given size",			      
			      _zsClusteringAlgo, string(EUTELESCOPE::SPARSECLUSTER));
  
  registerProcessorParameter ("ClusterSizeX",
			      "Maximum allowed cluster size along x (only odd numbers)",
			      _xClusterSize, static_cast<int> (5));

  registerProcessorParameter ("ClusterSizeY",
			      "Maximum allowed cluster size along y (only odd numbers)",
			      _yClusterSize, static_cast<int> (5));

  registerProcessorParameter ("SeedPixelCut",
			      "Threshold in SNR for seed pixel identification",
			      _seedPixelCut, static_cast<float> (4.5));
  
  registerProcessorParameter ("ClusterCut",
			      "Threshold in SNR for cluster identification",
			      _clusterCut, static_cast<float> (3.0));

  registerProcessorParameter("HistoInfoFileName", "This is the name of the histogram information file",
			     _histoInfoFileName, string( "histoinfo.xml" ) );


  registerProcessorParameter("ZSSeedCut","Threshold in SNR for seed pixel contained in ZS data",
			     _zsSeedCut, static_cast<float > (4.5));
  
  registerProcessorParameter("ZSClusterCut","Threshold in SNR for clusters contained in ZS data",
			     _zsClusterCut, static_cast<float > (3.0) );
  
  registerProcessorParameter("MinDistance","Minimum distance between sparsified pixel ( touching == sqrt(2)) ",
			     _minDistance, static_cast<float > (0.0 ) );
  

#ifdef MARLIN_USE_AIDA
  IntVec clusterNxNExample;
  clusterNxNExample.push_back(3);
  clusterNxNExample.push_back(5);
  
  registerOptionalParameter("ClusterNxN", "The list of cluster NxN to be filled."
			    "For example 3 means filling the 3x3 histogram spectrum",
			    _clusterSpectraNxNVector, clusterNxNExample);

  IntVec clusterNExample;
  clusterNExample.push_back(4);
  clusterNExample.push_back(9);
  clusterNExample.push_back(14);
  clusterNExample.push_back(19);
  clusterNExample.push_back(25);
  registerOptionalParameter("ClusterN", "The list of cluster N to be filled."
			    "For example 7 means filling the cluster spectra with the 7 most significant pixels",
			    _clusterSpectraNVector, clusterNExample );
#endif

  registerProcessorParameter("HistogramFilling","Switch on or off the histogram filling",
			     _fillHistos, static_cast< bool > ( true ) );


  _isFirstEvent = true;
}


void EUTelClusteringProcessor::init () {
  // this method is called only once even when the rewind is active
  // usually a good idea to
  printParameters ();

  // in the case the FIXEDFRAME algorithm is selected, the check if
  // the _xClusterSize and the _yClusterSize are odd numbers
  if ( _nzsClusteringAlgo == EUTELESCOPE::FIXEDFRAME ) {
    bool isZero = ( _xClusterSize <= 0 );
    bool isEven = ( _xClusterSize % 2 == 0 );
    if ( isZero || isEven ) {
      throw InvalidParameterException("_xClusterSize has to be positive and odd");
    }
    isZero = ( _yClusterSize <= 0 );
    isEven = ( _yClusterSize % 2 == 0 );
    if ( isZero || isEven ) {
      throw InvalidParameterException("_yClusterSize has to be positive and odd");
    }
  }

  // set to zero the run and event counters
  _iRun = 0;
  _iEvt = 0;



}

void EUTelClusteringProcessor::processRunHeader (LCRunHeader * rdr) {


  auto_ptr<EUTelRunHeaderImpl> runHeader( new EUTelRunHeaderImpl( rdr ) );

  runHeader->addProcessor( type());
  
  // the four vectors containing the first and the last pixel
  // along both the directions
  _minX = runHeader->getMinX();
  _maxX = runHeader->getMaxX();
  _minY = runHeader->getMinY();
  _maxY = runHeader->getMaxY();

#ifdef MARLIN_USE_AIDA
  // let me get from the run header all the available parameter
  _noOfDetector = runHeader->getNoOfDetector();
#endif



  // increment the run counter
  ++_iRun;

}


void EUTelClusteringProcessor::processEvent (LCEvent * event) {

  if (_iEvt % 10 == 0) 
    streamlog_out( MESSAGE4 ) << "Processing event " 
			      << setw(6) << setiosflags(ios::right) << event->getEventNumber() << " in run "
			      << setw(6) << setiosflags(ios::right) << setfill('0')  << event->getRunNumber() << setfill(' ')
			      << " (Total = " << setw(10) << _iEvt << ")" << resetiosflags(ios::left) << endl;
  ++_iEvt;

  
  
  // things to be done only once! 
  if ( isFirstEvent() ) {
    // prepare a vector to store the number of clusters found for 
    _totCluster.clear();
    _totCluster.resize( _noOfDetector, 0 );
  }


  // in the current event it is possible to have either full frame and
  // zs data. Here is the right place to guess what we have
  bool hasNZSData = true;
  try {
    event->getCollection(_nzsDataCollectionName);
    
  } catch (lcio::DataNotAvailableException& e) {
    hasNZSData = false;
    streamlog_out ( DEBUG4 ) << "No NZS data found in the event" << endl;
  }
  
  bool hasZSData = true;
  try {
    event->getCollection( _zsDataCollectionName ) ;
  } catch (lcio::DataNotAvailableException& e ) {
    hasZSData = false;
    streamlog_out ( DEBUG4 ) << "No ZS data found in the event" << endl;
  }

  if ( !hasNZSData && !hasZSData ) {
    streamlog_out ( MESSAGE2 ) << "The current event doesn't contain neither ZS nor NZS data collections" << endl
			       << "Leaving this event without any further processing" << endl;
    return ;
  }

    
#ifdef MARLIN_USE_AIDA
  // book the histograms now
  if ( _fillHistos && isFirstEvent() ) {
    bookHistos();
  }
#endif

  EUTelEventImpl * evt = static_cast<EUTelEventImpl*> (event);
  if ( evt->getEventType() == kEORE ) {
    streamlog_out ( DEBUG4 ) <<  "EORE found: nothing else to do." <<  endl;
    return;
  } else if ( evt->getEventType() == kUNKNOWN ) {
    streamlog_out ( WARNING2 ) << "Event number " << evt->getEventNumber() 
			       << " is of unknown type. Continue considering it as a normal Data Event." << endl;
  }

  // prepare a pulse collection to add all clusters found
  LCCollectionVec * pulseCollection = new LCCollectionVec(LCIO::TRACKERPULSE);

  // first look for cluster in RAW mode frames
  if ( hasNZSData ) {
    // put here all the possible algorithm applicable to NZS data
    if ( _nzsClusteringAlgo == EUTELESCOPE::FIXEDFRAME )     fixedFrameClustering(evt, pulseCollection);
  
  }
  if ( hasZSData ) {
    // put here all the possible algorithm applicable to ZS data 
    if ( _zsClusteringAlgo == EUTELESCOPE::SPARSECLUSTER )       sparseClustering(evt, pulseCollection);
    else if ( _zsClusteringAlgo == EUTELESCOPE::SPARSECLUSTER2 ) sparseClustering2(evt, pulseCollection);
    else if ( _zsClusteringAlgo == EUTELESCOPE::FIXEDFRAME )     zsFixedFrameClustering(evt, pulseCollection);

  }

  // if the pulseCollection is not empty add it to the event
  if ( pulseCollection->size() != 0 ) {
    evt->addCollection(pulseCollection,_pulseCollectionName);

#ifdef MARLIN_USE_AIDA
    if ( _fillHistos ) fillHistos(event);
#endif

  } else {
    delete pulseCollection;
  }

  _isFirstEvent = false;

}


void EUTelClusteringProcessor::zsFixedFrameClustering(LCEvent * evt, LCCollectionVec * pulseCollection) {

  streamlog_out ( DEBUG4 ) << "Looking for clusters in the zs data with FixedFrame algorithm " << endl;

  // get the collections of interest from the event.
  LCCollectionVec * zsInputCollectionVec  = dynamic_cast < LCCollectionVec * > (evt->getCollection( _zsDataCollectionName ));
  LCCollectionVec * noiseCollectionVec    = dynamic_cast < LCCollectionVec * > (evt->getCollection(_noiseCollectionName));
  LCCollectionVec * statusCollectionVec   = dynamic_cast < LCCollectionVec * > (evt->getCollection(_statusCollectionName));
  // prepare some decoders
  CellIDDecoder<TrackerDataImpl> cellDecoder( zsInputCollectionVec );
  CellIDDecoder<TrackerDataImpl> noiseDecoder( noiseCollectionVec );

  // this is the equivalent of the dummyCollection in the fixed frame
  // clustering. BTW we should consider changing that "meaningful"
  // name! This contains cluster and not yet pulses 
  auto_ptr<LCCollectionVec > sparseClusterCollectionVec ( new  LCCollectionVec(LCIO::TRACKERDATA) );
  CellIDEncoder<TrackerDataImpl> idZSClusterEncoder( EUTELESCOPE::ZSCLUSTERDEFAULTENCODING, sparseClusterCollectionVec.get()  );

  // prepare an encoder also for the pulse collection
  CellIDEncoder<TrackerPulseImpl> idZSPulseEncoder(EUTELESCOPE::PULSEDEFAULTENCODING, pulseCollection);
  
  // utility
  short limitExceed    = 0;
  
  if ( isFirstEvent() ) {

    // For the time being nothing to do specifically in the first
    // event.

  }

  for ( unsigned int i = 0 ; i < zsInputCollectionVec->size(); i++ ) {
    // get the TrackerData and guess which kind of sparsified data it
    // contains.
    TrackerDataImpl * zsData = dynamic_cast< TrackerDataImpl * > ( zsInputCollectionVec->getElementAt( i ) );
    SparsePixelType   type   = static_cast<SparsePixelType> ( static_cast<int> (cellDecoder( zsData )["sparsePixelType"]) );
    _iDetector               = static_cast<int > ( cellDecoder( zsData )["sensorID"] );

    // reset the cluster counter for the clusterID
    int clusterID = 0;
    
    // get the noise and the status matrix with the right detectorID
    TrackerDataImpl    * noise  = dynamic_cast<TrackerDataImpl*>   (noiseCollectionVec->getElementAt(_iDetector));
    TrackerRawDataImpl * status = dynamic_cast<TrackerRawDataImpl*>(statusCollectionVec->getElementAt(_iDetector));

    // prepare the matrix decoder
    EUTelMatrixDecoder matrixDecoder( noiseDecoder , noise );

    // prepare a data vector mimicking the TrackerData data of the
    // standard FixedFrameClustering. Initialize all the entries to zero.
    vector<float > dataVec( noise->getChargeValues().size(), 0. );

    // prepare a multimap for the seed candidates
    multimap<float , int > seedCandidateMap;

    if ( type == kEUTelSimpleSparsePixel ) {
      
      // now prepare the EUTelescope interface to sparsified data.
      auto_ptr<EUTelSparseDataImpl<EUTelSimpleSparsePixel > > 
	sparseData(new EUTelSparseDataImpl<EUTelSimpleSparsePixel> ( zsData ));
      
      streamlog_out ( DEBUG1 ) << "Processing sparse data on detector " << _iDetector << " with "
			       << sparseData->size() << " pixels " << endl;
     
      // loop over all pixels in the sparseData object.
      auto_ptr<EUTelSimpleSparsePixel > sparsePixel( new EUTelSimpleSparsePixel );
      for ( unsigned int iPixel = 0; iPixel < sparseData->size(); iPixel++ ) {
	sparseData->getSparsePixelAt( iPixel, sparsePixel.get() );
	int   index  = matrixDecoder.getIndexFromXY( sparsePixel->getXCoord(), sparsePixel->getYCoord() );
	float signal = sparsePixel->getSignal();
	dataVec[ index  ] = signal;
	if (  ( signal  > _seedPixelCut * noise->getChargeValues()[ index ] ) &&
	      ( status->getADCValues()[ index ] == EUTELESCOPE::GOODPIXEL ) ) {
	  seedCandidateMap.insert ( make_pair ( signal, index ) );
	}
	  
      }
    } else {
      throw UnknownDataTypeException("Unknown sparsified pixel");
    }
    
    if ( seedCandidateMap.size() != 0 ) {
      
      streamlog_out ( DEBUG0 ) << "  Seed candidates " << _seedCandidateMap.size() << endl;
      
      // now built up a cluster for each seed candidate
      multimap<float, int >::reverse_iterator rMapIter = seedCandidateMap.rbegin();
      while ( rMapIter != seedCandidateMap.rend() ) {
	if ( status->adcValues()[ (*rMapIter).second ] == EUTELESCOPE::GOODPIXEL ) {
	  // if we enter here, this means that at least the seed pixel
	  // wasn't added yet to another cluster.  Note that now we need
	  // to build a candidate cluster that has to pass the
	  // clusterCut to be considered a good cluster
	  double clusterCandidateSignal    = 0.;
	  double clusterCandidateNoise2    = 0.;
	  FloatVec clusterCandidateCharges;
	  IntVec   clusterCandidateIndeces;
	  int seedX, seedY;
	  matrixDecoder.getXYFromIndex ( (*rMapIter).second, seedX, seedY );
	  
	  // start looping around the seed pixel. Remember that the seed
	  // pixel has to stay in the center of cluster
	  ClusterQuality cluQuality = kGoodCluster;
	  for (int yPixel = seedY - (_yClusterSize / 2); yPixel <= seedY + (_yClusterSize / 2); yPixel++) {
	    for (int xPixel =  seedX - (_xClusterSize / 2); xPixel <= seedX + (_xClusterSize / 2); xPixel++) {
	      // always check we are still within the sensor!!!
	      if ( ( xPixel >= _minX[_iDetector] )  &&  ( xPixel <= _maxX[_iDetector] ) &&
		   ( yPixel >= _minY[_iDetector] )  &&  ( yPixel <= _maxY[_iDetector] ) ) {
		int index = matrixDecoder.getIndexFromXY(xPixel, yPixel);
		
		bool isHit  = ( status->getADCValues()[index] == EUTELESCOPE::HITPIXEL  );
		bool isGood = ( status->getADCValues()[index] == EUTELESCOPE::GOODPIXEL );
		if ( isGood && !isHit ) {
		  // if the pixel wasn't selected, then its signal
		  // will be 0.0. Mark it in the status
		  if ( dataVec[ index ] == 0.0 ) 
		    status->adcValues()[ index ] == EUTELESCOPE::MISSINGPIXEL ;
		  clusterCandidateSignal += dataVec[ index ] ;
		  clusterCandidateNoise2 += pow ( noise->getChargeValues() [ index ], 2 );
		  clusterCandidateCharges.push_back( dataVec[ index ] );
		  clusterCandidateIndeces.push_back( index );
		} else if ( isHit ) {
		  // this can be a good place to flag the current
		  // cluster as kMergedCluster, but it would introduce
		  // a bias since the at least another cluster (the
		  // one which this pixel belong to) is not flagged.
		  //
		  // In order to flag all merged clusters and possibly
		  // try to separate the different contributions use
		  // the EUTelSeparateClusterProcessor. In this
		  // processor not all the merged clusters will be
		  // flagged as kMergedCluster | kIncompleteCluster
		  cluQuality = cluQuality | kIncompleteCluster | kMergedCluster ; 
		  clusterCandidateCharges.push_back(0.);
		} else if ( !isGood ) {
		  cluQuality = cluQuality | kIncompleteCluster;
		  clusterCandidateCharges.push_back(0.);
		}
	      } else {
		cluQuality = cluQuality | kBorderCluster;
		clusterCandidateCharges.push_back(0.);
	      }
	    }
	  }
	  // at this point we have built the cluster candidate,
	  // we need to validate it
	  if ( clusterCandidateSignal > _clusterCut * sqrt( clusterCandidateNoise2 ) ) {
	    // the cluster candidate is a good cluster
	    // mark all pixels belonging to the cluster as hit
	    IntVec::iterator indexIter = clusterCandidateIndeces.begin();
	    
	    // the final result of the clustering will enter in a
	    // TrackerPulseImpl in order to be algorithm independent 
	    TrackerPulseImpl * pulse = new TrackerPulseImpl;
	    CellIDEncoder<TrackerPulseImpl> idPulseEncoder(EUTELESCOPE::PULSEDEFAULTENCODING, pulseCollection);
	    idPulseEncoder["sensorID"]      = _iDetector;
	    idPulseEncoder["clusterID"]     = clusterID;
	    idPulseEncoder["xSeed"]         = seedX;
	    idPulseEncoder["ySeed"]         = seedY;
	    idPulseEncoder["xCluSize"]      = _xClusterSize;
	    idPulseEncoder["yCluSize"]      = _yClusterSize;
	    idPulseEncoder["type"]          = static_cast<int>(kEUTelFFClusterImpl);
	    idPulseEncoder.setCellID(pulse);	    
	    
	    TrackerDataImpl * cluster = new TrackerDataImpl;
	    CellIDEncoder<TrackerDataImpl> idClusterEncoder(EUTELESCOPE::CLUSTERDEFAULTENCODING, sparseClusterCollectionVec.get());
	    idClusterEncoder["sensorID"]      = _iDetector;
	    idClusterEncoder["clusterID"]     = clusterID;
	    idClusterEncoder["xSeed"]         = seedX;
	    idClusterEncoder["ySeed"]         = seedY;
	    idClusterEncoder["xCluSize"]      = _xClusterSize;
	    idClusterEncoder["yCluSize"]      = _yClusterSize;
	    idClusterEncoder["quality"]       = static_cast<int>(cluQuality);
	    idClusterEncoder.setCellID(cluster);
	      

	    streamlog_out (DEBUG0) << "  Cluster no " <<  clusterID << " seedX " << seedX << " seedY " << seedY << endl;
	      
	      
	    while ( indexIter != clusterCandidateIndeces.end() ) {
	      status->adcValues()[(*indexIter)] = EUTELESCOPE::HITPIXEL;
	      ++indexIter;
	    }

	      
	    // copy the candidate charges inside the cluster
	    cluster->setChargeValues(clusterCandidateCharges);
	    sparseClusterCollectionVec->push_back(cluster);

	    EUTelFFClusterImpl * eutelCluster = new EUTelFFClusterImpl( cluster );
	    pulse->setCharge(eutelCluster->getTotalCharge());
	    delete eutelCluster;
	      
	    pulse->setQuality(static_cast<int>(cluQuality));
	    pulse->setTrackerData(cluster);
	    pulseCollection->push_back(pulse);
	      
	    // increment the cluster counters
	    _totCluster[_iDetector] += 1;
	    ++clusterID;
	    if ( clusterID >= 256 ) {
	      ++limitExceed;
	      --clusterID;
	      streamlog_out ( WARNING0 ) << "Event " << _iEvt << " contains more than 256 clusters (" 
					 << clusterID + limitExceed << ")" << endl;
	    }
	  } 
	}
	++rMapIter;
      }
    }   
  }

  // if the sparseClusterCollectionVec isn't empty add it to the
  // current event. The pulse collection will be added afterwards 
  if ( sparseClusterCollectionVec->size() != 0 ) {
    evt->addCollection( sparseClusterCollectionVec.release(), "original_zsdata" );
  }

}     



void EUTelClusteringProcessor::sparseClustering(LCEvent * evt, LCCollectionVec * pulseCollection) {

  streamlog_out ( DEBUG4 ) << "Looking for clusters in the zs data with SparseCluster algorithm " << endl;
  
  // get the collections of interest from the event.
  LCCollectionVec * zsInputCollectionVec  = dynamic_cast < LCCollectionVec * > (evt->getCollection( _zsDataCollectionName ));
  LCCollectionVec * noiseCollectionVec    = dynamic_cast < LCCollectionVec * > (evt->getCollection(_noiseCollectionName));
  
  // prepare some decoders
  CellIDDecoder<TrackerDataImpl> cellDecoder( zsInputCollectionVec );
  CellIDDecoder<TrackerDataImpl> noiseDecoder( noiseCollectionVec );

  // this is the equivalent of the dummyCollection in the fixed frame
  // clustering. BTW we should consider changing that "meaningful"
  // name! This contains cluster and not yet pulses 
  auto_ptr<LCCollectionVec > sparseClusterCollectionVec ( new  LCCollectionVec(LCIO::TRACKERDATA) );
  CellIDEncoder<TrackerDataImpl> idZSClusterEncoder( EUTELESCOPE::ZSCLUSTERDEFAULTENCODING, sparseClusterCollectionVec.get()  );

  // prepare an encoder also for the pulse collection
  CellIDEncoder<TrackerPulseImpl> idZSPulseEncoder(EUTELESCOPE::PULSEDEFAULTENCODING, pulseCollection);

  // utility
  short limitExceed    = 0;

  if ( isFirstEvent() ) {

    // For the time being nothing to do specifically in the first
    // event.

  }
  
  // in the zsInputCollectionVec we should have one TrackerData for
  // each detector working in ZS mode. We need to loop over all of
  // them
  for ( unsigned int i = 0 ; i < zsInputCollectionVec->size(); i++ ) {
    // get the TrackerData and guess which kind of sparsified data it
    // contains.
    TrackerDataImpl * zsData = dynamic_cast< TrackerDataImpl * > ( zsInputCollectionVec->getElementAt( i ) );
    SparsePixelType   type   = static_cast<SparsePixelType> ( static_cast<int> (cellDecoder( zsData )["sparsePixelType"]) );
    _iDetector               = static_cast<int > ( cellDecoder( zsData )["sensorID"] );
    
    // reset the cluster counter for the clusterID
    int clusterID = 0;
    
    // get the noise matrix with the right detectorID
    TrackerDataImpl    * noise  = dynamic_cast<TrackerDataImpl*>   (noiseCollectionVec->getElementAt(_iDetector));
    
    // prepare the matrix decoder
    EUTelMatrixDecoder matrixDecoder( noiseDecoder , noise );


    
    if ( type == kEUTelSimpleSparsePixel ) {
      
      // now prepare the EUTelescope interface to sparsified data.
      auto_ptr<EUTelSparseDataImpl<EUTelSimpleSparsePixel > > 
	sparseData(new EUTelSparseDataImpl<EUTelSimpleSparsePixel> ( zsData ));
      
      streamlog_out ( DEBUG1 ) << "Processing sparse data on detector " << _iDetector << " with "
			       << sparseData->size() << " pixels " << endl;
      
      // get from the sparse data the list of neighboring pixels
      list<list< unsigned int> > listOfList = sparseData->findNeighborPixels( _minDistance );

      // prepare a vector to store the noise values
      vector<float > noiseValueVec;
      
      // prepare a generic pixel to store the values
      EUTelSimpleSparsePixel * pixel = new EUTelSimpleSparsePixel;
      
      // now loop over all the lists
      list<list< unsigned int> >::iterator listOfListIter = listOfList.begin();

      while ( listOfListIter != listOfList.end() ) {
	list<unsigned int > currentList = (*listOfListIter);

	// prepare a TrackerData to store the cluster candidate
	auto_ptr< TrackerDataImpl > zsCluster ( new TrackerDataImpl );
	
	// prepare a reimplementation of sparsified cluster
	auto_ptr<EUTelSparseClusterImpl<EUTelSimpleSparsePixel > > 
	  sparseCluster ( new EUTelSparseClusterImpl<EUTelSimpleSparsePixel > ( zsCluster.get()  ) );
	
	// clear the noise vector
	noiseValueVec.clear();
	
	// now we can finally build the cluster candidate
	list<unsigned int >::iterator listIter = currentList.begin();
	
	while ( listIter != currentList.end() ) {
	  
	  sparseData->getSparsePixelAt( (*listIter ), pixel );
	  sparseCluster->addSparsePixel( pixel );
	  
	  noiseValueVec.push_back(noise->getChargeValues()[ matrixDecoder.getIndexFromXY ( pixel->getXCoord(), pixel->getYCoord() ) ]);

	  // remember the iterator++
	  ++listIter;
	}
	sparseCluster->setNoiseValues( noiseValueVec );

	// verify if the cluster candidates can become a good cluster
	if ( ( sparseCluster->getSeedSNR() >= _zsSeedCut ) && 
	     ( sparseCluster->getClusterSNR() >= _zsClusterCut ) ) {

	  // ok good cluster....
	  // set the ID for this zsCluster
	  idZSClusterEncoder["sensorID"] = _iDetector;
	  idZSClusterEncoder["clusterID"] = clusterID;
	  idZSClusterEncoder["sparsePixelType"] = static_cast<int> ( type );
	  idZSClusterEncoder["quality"] = 0;
	  idZSClusterEncoder.setCellID( zsCluster.get() );
	  
	  // add it to the cluster collection
	  sparseClusterCollectionVec->push_back( zsCluster.get() );

	  // prepare a pulse for this cluster
	  int xSeed, ySeed, xSize, ySize;
	  sparseCluster->getSeedCoord(xSeed, ySeed);
	  sparseCluster->getClusterSize(xSize, ySize);

	  auto_ptr<TrackerPulseImpl> zsPulse ( new TrackerPulseImpl );
	  idZSPulseEncoder["sensorID"]  = _iDetector;
	  idZSPulseEncoder["clusterID"] = clusterID;
	  idZSPulseEncoder["xSeed"]     = xSeed;
	  idZSPulseEncoder["ySeed"]     = ySeed;
	  idZSPulseEncoder["xCluSize"]  = xSize;
	  idZSPulseEncoder["yCluSize"]  = ySize;
	  idZSPulseEncoder["type"]      = static_cast<int>(kEUTelSparseClusterImpl);
	  idZSPulseEncoder.setCellID( zsPulse.get() );

	  zsPulse->setCharge( sparseCluster->getTotalCharge() );
	  zsPulse->setQuality( static_cast<int > (sparseCluster->getClusterQuality()) );
	  zsPulse->setTrackerData( zsCluster.release() );
	  pulseCollection->push_back( zsPulse.release() );
	

	  // last but not least increment the clusterID
	  _totCluster[_iDetector] += 1;
	  ++clusterID;
	  if ( clusterID > 256 ) {
	    --clusterID;
	    ++limitExceed;
	    streamlog_out ( WARNING2 ) << "Event " << evt->getEventNumber() << " on run " << evt->getRunNumber()
				       << " contains more than 256 cluster (" << clusterID + limitExceed << ")" << endl;
	  }

	} else {
	  
	  // in the case the cluster candidate is not passing the
	  // threshold ... forget about ! ! !
	  // memory should be automatically cleaned by auto_ptr's

	}
 
	// remember to increment the iterator
	++listOfListIter;
      }

      // clean up the memory
      delete pixel;
      
    } else {
      throw UnknownDataTypeException("Unknown sparsified pixel");
    }
    
    
    

  } // this is the end of the loop over all ZS detectors
  
  // if the sparseClusterCollectionVec isn't empty add it to the
  // current event. The pulse collection will be added afterwards 
  if ( sparseClusterCollectionVec->size() != 0 ) {
    evt->addCollection( sparseClusterCollectionVec.release(), "original_zsdata" );
  }

}

void EUTelClusteringProcessor::sparseClustering2(LCEvent * evt, LCCollectionVec * pulseCollection) {
  
  streamlog_out ( DEBUG4 ) << "Looking for clusters in the zs data with SparseCluster2 algorithm " << endl;

  // get the collections of interest from the event.
  LCCollectionVec * zsInputCollectionVec  = dynamic_cast < LCCollectionVec * > (evt->getCollection( _zsDataCollectionName ));
  LCCollectionVec * noiseCollectionVec    = dynamic_cast < LCCollectionVec * > (evt->getCollection(_noiseCollectionName));
  
  // prepare some decoders
  CellIDDecoder<TrackerDataImpl> cellDecoder( zsInputCollectionVec );
  CellIDDecoder<TrackerDataImpl> noiseDecoder( noiseCollectionVec );

  // this is the equivalent of the dummyCollection in the fixed frame
  // clustering. BTW we should consider changing that "meaningful"
  // name! This contains cluster and not yet pulses 
  auto_ptr<LCCollectionVec > sparseClusterCollectionVec ( new  LCCollectionVec(LCIO::TRACKERDATA) );
  CellIDEncoder<TrackerDataImpl> idZSClusterEncoder( EUTELESCOPE::ZSCLUSTERDEFAULTENCODING, sparseClusterCollectionVec.get()  );

  // prepare an encoder also for the pulse collection
  CellIDEncoder<TrackerPulseImpl> idZSPulseEncoder(EUTELESCOPE::PULSEDEFAULTENCODING, pulseCollection);

  // utility
  short limitExceed    = 0;

  if ( isFirstEvent() ) {

    // For the time being nothing to do specifically in the first
    // event.

  }
  
  // in the zsInputCollectionVec we should have one TrackerData for
  // each detector working in ZS mode. We need to loop over all of
  // them
  for ( unsigned int i = 0 ; i < zsInputCollectionVec->size(); i++ ) {
    // get the TrackerData and guess which kind of sparsified data it
    // contains.
    TrackerDataImpl * zsData = dynamic_cast< TrackerDataImpl * > ( zsInputCollectionVec->getElementAt( i ) );
    SparsePixelType   type   = static_cast<SparsePixelType> ( static_cast<int> (cellDecoder( zsData )["sparsePixelType"]) );
    _iDetector               = static_cast<int > ( cellDecoder( zsData )["sensorID"] );
    
    // reset the cluster counter for the clusterID
    int clusterID = 0;
    
    // get the noise matrix with the right detectorID
    TrackerDataImpl    * noise  = dynamic_cast<TrackerDataImpl*>   (noiseCollectionVec->getElementAt(_iDetector));
    
    // prepare the matrix decoder
    EUTelMatrixDecoder matrixDecoder( noiseDecoder , noise );

    if ( type == kEUTelSimpleSparsePixel ) {
      
      // now prepare the EUTelescope interface to sparsified data.
      auto_ptr<EUTelSparseData2Impl<EUTelSimpleSparsePixel > > 
	sparseData(new EUTelSparseData2Impl<EUTelSimpleSparsePixel> ( zsData ));

      streamlog_out ( DEBUG2 ) << "Processing sparse data on detector " << _iDetector << " with "
			       << sparseData->size() << " pixels " << endl;

      // get from the sparse data the list of neighboring pixels
      list<list< unsigned int> > listOfList = sparseData->findNeighborPixels( _minDistance );

      // prepare a vector to store the noise values
      vector<float > noiseValueVec;
      
      // prepare a generic pixel to store the values
      EUTelSimpleSparsePixel * pixel = new EUTelSimpleSparsePixel;
      
      // now loop over all the lists
      list<list< unsigned int> >::iterator listOfListIter = listOfList.begin();

      while ( listOfListIter != listOfList.end() ) {
	list<unsigned int > currentList = (*listOfListIter);

	// prepare a TrackerData to store the cluster candidate
	auto_ptr< TrackerDataImpl > zsCluster ( new TrackerDataImpl );
	
	// prepare a reimplementation of sparsified cluster
	auto_ptr<EUTelSparseClusterImpl<EUTelSimpleSparsePixel > > 
	  sparseCluster ( new EUTelSparseClusterImpl<EUTelSimpleSparsePixel > ( zsCluster.get()  ) );
	
	// clear the noise vector
	noiseValueVec.clear();
	
	// now we can finally build the cluster candidate
	list<unsigned int >::iterator listIter = currentList.begin();
	
	while ( listIter != currentList.end() ) {
	 
	  sparseData->getSparsePixelSortedAt( (*listIter ), pixel );
	  sparseCluster->addSparsePixel( pixel );
	  noiseValueVec.push_back(noise->getChargeValues()[ matrixDecoder.getIndexFromXY ( pixel->getXCoord(), pixel->getYCoord() ) ]);
	  
	  // remember the iterator++
	  ++listIter;
	}
	sparseCluster->setNoiseValues( noiseValueVec );

	// verify if the cluster candidates can become a good cluster
	if ( ( sparseCluster->getSeedSNR() >= _zsSeedCut ) && 
	     ( sparseCluster->getClusterSNR() >= _zsClusterCut ) ) {


	  // ok good cluster....
	  // set the ID for this zsCluster
	  idZSClusterEncoder["sensorID"] = _iDetector;
	  idZSClusterEncoder["clusterID"] = clusterID;
	  idZSClusterEncoder["sparsePixelType"] = static_cast<int> ( type );
	  idZSClusterEncoder["quality"] = 0;
	  idZSClusterEncoder.setCellID( zsCluster.get() );
	  
	  // add it to the cluster collection
	  sparseClusterCollectionVec->push_back( zsCluster.get() );

	  // prepare a pulse for this cluster
	  int xSeed, ySeed, xSize, ySize;
	  sparseCluster->getSeedCoord(xSeed, ySeed);
	  sparseCluster->getClusterSize(xSize, ySize);

	  auto_ptr<TrackerPulseImpl> zsPulse ( new TrackerPulseImpl );
	  idZSPulseEncoder["sensorID"]  = _iDetector;
	  idZSPulseEncoder["clusterID"] = clusterID;
	  idZSPulseEncoder["xSeed"]     = xSeed;
	  idZSPulseEncoder["ySeed"]     = ySeed;
	  idZSPulseEncoder["xCluSize"]  = (xSize < 32 ? xSize : 31 );
	  idZSPulseEncoder["yCluSize"]  = (ySize < 32 ? ySize : 31 );
	  idZSPulseEncoder["type"]      = static_cast<int>(kEUTelSparseClusterImpl);
	  idZSPulseEncoder.setCellID( zsPulse.get() );

	  zsPulse->setCharge( sparseCluster->getTotalCharge() );
	  zsPulse->setQuality( static_cast<int > (sparseCluster->getClusterQuality()) );
	  zsPulse->setTrackerData( zsCluster.release() );
	  pulseCollection->push_back( zsPulse.release() );
	

	  // last but not least increment the clusterID
	  _totCluster[_iDetector] += 1;
	  ++clusterID;
	  if ( clusterID > 256 ) {
	    --clusterID;
	    ++limitExceed;
	    streamlog_out ( WARNING2 ) << "Event " << evt->getEventNumber() << " on run " << evt->getRunNumber()
				       << " contains more than 256 cluster (" << clusterID + limitExceed << ")" << endl;
	  }

	} else {
	  
	  // in the case the cluster candidate is not passing the
	  // threshold ... forget about ! ! !
	  // memory should be automatically cleaned by auto_ptr's

	}
 
	// remember to increment the iterator
	++listOfListIter;
      }

      // clean up the memory
      delete pixel;
      
    } else {
      throw UnknownDataTypeException("Unknown sparsified pixel");
    }
    
    
    

  } // this is the end of the loop over all ZS detectors
  
  // if the sparseClusterCollectionVec isn't empty add it to the
  // current event. The pulse collection will be added afterwards 
  if ( sparseClusterCollectionVec->size() != 0 ) {
    evt->addCollection( sparseClusterCollectionVec.release(), "original_zsdata" );
  }

}

void EUTelClusteringProcessor::fixedFrameClustering(LCEvent * evt, LCCollectionVec * pulseCollection) {
  
  streamlog_out ( DEBUG4 ) << "Looking for clusters in the RAW frame with FixedFrame algorithm " << endl;

  LCCollectionVec * nzsInputCollectionVec = dynamic_cast < LCCollectionVec * > (evt->getCollection(_nzsDataCollectionName));
  LCCollectionVec * noiseCollectionVec    = dynamic_cast < LCCollectionVec * > (evt->getCollection(_noiseCollectionName));
  LCCollectionVec * statusCollectionVec   = dynamic_cast < LCCollectionVec * > (evt->getCollection(_statusCollectionName));

  CellIDDecoder<TrackerDataImpl> cellDecoder( nzsInputCollectionVec );

  if (isFirstEvent()) {
    
    // check if for each TrackerData into the NZS corresponds one
    // TrackerData with noise information having the same number of
    // pixels. 

    for ( unsigned int i = 0 ; i < nzsInputCollectionVec->size(); i++ ) {
      TrackerDataImpl    * nzsData = dynamic_cast<TrackerDataImpl* > ( nzsInputCollectionVec->getElementAt( i ) );
      int detectorID     = cellDecoder( nzsData ) ["sensorID"];

      TrackerDataImpl    * noise   = dynamic_cast<TrackerDataImpl* >    ( noiseCollectionVec->getElementAt( detectorID ) );
      TrackerRawDataImpl * status  = dynamic_cast<TrackerRawDataImpl *> ( statusCollectionVec->getElementAt( detectorID ) );

      if ( ( noise->chargeValues().size() != status->adcValues().size() ) ||
	   ( noise->chargeValues().size() != nzsData->chargeValues().size() ) ) {
	throw IncompatibleDataSetException("NZS data and noise/status size mismatch");
      }
    }
    
    
    /// /* DEBUG */    logfile.open("clustering.log");
  }
  
#ifdef MARLINDEBUG  
  /// /* DEBUG */ message<DEBUG> ( logfile << "Event " << _iEvt );
#endif
  
  streamlog_out ( DEBUG0 ) << "Event " << _iEvt << endl;
  
  LCCollectionVec * dummyCollection = new LCCollectionVec(LCIO::TRACKERDATA);
  
  for ( int i = 0; i < nzsInputCollectionVec->getNumberOfElements(); i++) {
        
    // get the calibrated data 
    TrackerDataImpl    * nzsData = dynamic_cast<TrackerDataImpl*>  (nzsInputCollectionVec->getElementAt( i ) );
    _iDetector = cellDecoder( nzsData ) ["sensorID"];
  
    streamlog_out ( DEBUG0 ) << "  Working on detector " << _iDetector << endl;

#ifdef MARLINDEBUG
    /// /* DEBUG */ message<DEBUG> ( logfile << "  Working on detector " << _iDetector );
#endif      

    TrackerDataImpl    * noise  = dynamic_cast<TrackerDataImpl*>   (noiseCollectionVec->getElementAt(_iDetector));
    TrackerRawDataImpl * status = dynamic_cast<TrackerRawDataImpl*>(statusCollectionVec->getElementAt(_iDetector));

    // prepare the matrix decoder
    EUTelMatrixDecoder matrixDecoder(cellDecoder, nzsData);

    // reset the status
    resetStatus(status);

    // initialize the cluster counter 
    short clusterCounter = 0;
    short limitExceed    = 0;

    _seedCandidateMap.clear();

#ifdef MARLINDEBUG    
    /// /* DEBUG */ message<DEBUG> ( log() << "Max signal " << (*max_element(nzsData->getChargeValues().begin(), nzsData->getChargeValues().end()))
    /// /* DEBUG */		     << "\nMin signal " << (*min_element(nzsData->getChargeValues().begin(), nzsData->getChargeValues().end())) );
#endif

    for (unsigned int iPixel = 0; iPixel < nzsData->getChargeValues().size(); iPixel++) {
      if (status->getADCValues()[iPixel] == EUTELESCOPE::GOODPIXEL) {
	if ( nzsData->getChargeValues()[iPixel] > _seedPixelCut * noise->getChargeValues()[iPixel]) {
	  _seedCandidateMap.insert(make_pair( nzsData->getChargeValues()[iPixel], iPixel));
	}
      }
    }

    // continue only if seed candidate map is not empty!
    if ( _seedCandidateMap.size() != 0 ) {

#ifdef MARLINDEBUG
      /// /* DEBUG */      message<DEBUG> ( logfile << "  Seed candidates " << _seedCandidateMap.size() ); 
#endif
      streamlog_out ( DEBUG0 ) << "  Seed candidates " << _seedCandidateMap.size() << endl;

      // now built up a cluster for each seed candidate 
      map<float, unsigned int>::iterator mapIter = _seedCandidateMap.end();     
      while ( mapIter != _seedCandidateMap.begin() ) {
	--mapIter;	
	// check if this seed candidate has not been already added to a
	// cluster
	if ( status->adcValues()[(*mapIter).second] == EUTELESCOPE::GOODPIXEL ) {
	  // if we enter here, this means that at least the seed pixel
	  // wasn't added yet to another cluster.  Note that now we need
	  // to build a candidate cluster that has to pass the
	  // clusterCut to be considered a good cluster
	  double clusterCandidateSignal    = 0.;
	  double clusterCandidateNoise2    = 0.;
	  FloatVec clusterCandidateCharges;
	  IntVec   clusterCandidateIndeces;
	  int seedX, seedY;
	  matrixDecoder.getXYFromIndex((*mapIter).second,seedX, seedY);

	  // start looping around the seed pixel. Remember that the seed
	  // pixel has to stay in the center of cluster
	  ClusterQuality cluQuality = kGoodCluster;
	  for (int yPixel = seedY - (_yClusterSize / 2); yPixel <= seedY + (_yClusterSize / 2); yPixel++) {
	    for (int xPixel =  seedX - (_xClusterSize / 2); xPixel <= seedX + (_xClusterSize / 2); xPixel++) {
	      // always check we are still within the sensor!!!
	      if ( ( xPixel >= _minX[_iDetector] )  &&  ( xPixel <= _maxX[_iDetector] ) &&
		   ( yPixel >= _minY[_iDetector] )  &&  ( yPixel <= _maxY[_iDetector] ) ) {
		int index = matrixDecoder.getIndexFromXY(xPixel, yPixel);

		bool isHit  = ( status->getADCValues()[index] == EUTELESCOPE::HITPIXEL  );
		bool isGood = ( status->getADCValues()[index] == EUTELESCOPE::GOODPIXEL );
		if ( isGood && !isHit ) {
		  clusterCandidateSignal += nzsData->getChargeValues()[index];
		  clusterCandidateNoise2 += pow(noise->getChargeValues()[index] , 2);
		  clusterCandidateCharges.push_back(nzsData->getChargeValues()[index]);
		  clusterCandidateIndeces.push_back(index);
		} else if (isHit) {
		  // this can be a good place to flag the current
		  // cluster as kMergedCluster, but it would introduce
		  // a bias since the at least another cluster (the
		  // one which this pixel belong to) is not flagged.
		  //
		  // In order to flag all merged clusters and possibly
		  // try to separate the different contributions use
		  // the EUTelSeparateClusterProcessor. In this
		  // processor not all the merged clusters will be
		  // flagged as kMergedCluster | kIncompleteCluster
		  cluQuality = cluQuality | kIncompleteCluster | kMergedCluster ; 
		  clusterCandidateCharges.push_back(0.);
		} else if (!isGood) {
		  cluQuality = cluQuality | kIncompleteCluster;
		  clusterCandidateCharges.push_back(0.);
		}
	      } else {
		cluQuality = cluQuality | kBorderCluster;
		clusterCandidateCharges.push_back(0.);
	      }
	    }
	  }
	  // at this point we have built the cluster candidate,
	  // we need to validate it
	  if ( clusterCandidateSignal > _clusterCut * sqrt(clusterCandidateNoise2) ) {
	    // the cluster candidate is a good cluster
	    // mark all pixels belonging to the cluster as hit
	    IntVec::iterator indexIter = clusterCandidateIndeces.begin();

	    // the final result of the clustering will enter in a
	    // TrackerPulseImpl in order to be algorithm independent 
	    TrackerPulseImpl * pulse = new TrackerPulseImpl;
	    CellIDEncoder<TrackerPulseImpl> idPulseEncoder(EUTELESCOPE::PULSEDEFAULTENCODING, pulseCollection);
	    idPulseEncoder["sensorID"]      = _iDetector;
	    idPulseEncoder["clusterID"]     = clusterCounter;
	    idPulseEncoder["xSeed"]         = seedX;
	    idPulseEncoder["ySeed"]         = seedY;
	    idPulseEncoder["xCluSize"]      = _xClusterSize;
	    idPulseEncoder["yCluSize"]      = _yClusterSize;
	    idPulseEncoder["type"]          = static_cast<int>(kEUTelFFClusterImpl);
	    idPulseEncoder.setCellID(pulse);	    


	    TrackerDataImpl * cluster = new TrackerDataImpl;
	    CellIDEncoder<TrackerDataImpl> idClusterEncoder(EUTELESCOPE::CLUSTERDEFAULTENCODING, dummyCollection);
	    idClusterEncoder["sensorID"]      = _iDetector;
	    idClusterEncoder["clusterID"]     = clusterCounter;
	    idClusterEncoder["xSeed"]         = seedX;
	    idClusterEncoder["ySeed"]         = seedY;
	    idClusterEncoder["xCluSize"]      = _xClusterSize;
	    idClusterEncoder["yCluSize"]      = _yClusterSize;
	    idClusterEncoder["quality"]       = static_cast<int>(cluQuality);
	    idClusterEncoder.setCellID(cluster);

#ifdef MARLINDEBUG
	    /// /* DEBUG */	    message<DEBUG> ( logfile << "  Cluster no " <<  clusterCounter << " seedX " << seedX << " seedY " << seedY );
#endif	    
	    streamlog_out (DEBUG0) << "  Cluster no " <<  clusterCounter << " seedX " << seedX << " seedY " << seedY << endl;

	    
	    while ( indexIter != clusterCandidateIndeces.end() ) {
	      status->adcValues()[(*indexIter)] = EUTELESCOPE::HITPIXEL;
	      ++indexIter;
	    }

#ifdef MARLINDEBUG
	    for (unsigned int iPixel = 0; iPixel < clusterCandidateIndeces.size(); iPixel++) {
	      /// /* DEBUG */	      message<DEBUG> ( logfile << "  x " << matrixDecoder.getXFromIndex(clusterCandidateIndeces[iPixel])
	      /// /* DEBUG */			       << "  y " <<  matrixDecoder.getYFromIndex(clusterCandidateIndeces[iPixel])
	      /// /* DEBUG */			       << "  s " <<  clusterCandidateCharges[iPixel]);
	      streamlog_out ( DEBUG0 ) << "  x " <<  matrixDecoder.getXFromIndex(clusterCandidateIndeces[iPixel])
				       << "  y " <<  matrixDecoder.getYFromIndex(clusterCandidateIndeces[iPixel])
				       << "  s " <<  clusterCandidateCharges[iPixel] << endl;
	    }
#endif

	    // copy the candidate charges inside the cluster
	    cluster->setChargeValues(clusterCandidateCharges);
	    dummyCollection->push_back(cluster);
	    
	    EUTelFFClusterImpl * eutelCluster = new EUTelFFClusterImpl( cluster );
	    pulse->setCharge(eutelCluster->getTotalCharge());
	    delete eutelCluster;

	    pulse->setQuality(static_cast<int>(cluQuality));
	    pulse->setTrackerData(cluster);
	    pulseCollection->push_back(pulse);

	    // increment the cluster counters
	    _totCluster[_iDetector] += 1;
	    ++clusterCounter;
	    if ( clusterCounter > 256 ) {
	      ++limitExceed;
	      --clusterCounter;
	      streamlog_out ( WARNING0 ) << "Event " << _iEvt << " contains more than 256 clusters (" 
					 << clusterCounter + limitExceed << ")" << endl;
	    }
	  } else {
	    // the cluster has not passed the cut!

	  }
	}
      }
    }
  }

  if ( dummyCollection->size() != 0 ) {
    evt->addCollection(dummyCollection,_dummyCollectionName);
  } else {
    delete dummyCollection;
  }
  
}



void EUTelClusteringProcessor::check (LCEvent * evt) {
  // nothing to check here - could be used to fill check plots in reconstruction processor
}
 

void EUTelClusteringProcessor::end() {

  streamlog_out ( MESSAGE2 ) <<  "Successfully finished" << endl;

  for (_iDetector = 0; _iDetector < (signed) _totCluster.size() ; _iDetector++) {
#ifdef MARLINDEBUG
    /// /* DEBUG */    message<DEBUG> ( logfile << "Found " << _totCluster[_iDetector] << " clusters on detector " << _iDetector );
#endif
    streamlog_out ( MESSAGE2 ) << "Found " << _totCluster[_iDetector] << " clusters on detector " << _iDetector << endl;
  }
#ifdef MARLINDEBUG
  /// /* DEBUG */  logfile.close();
#endif

}


void EUTelClusteringProcessor::resetStatus(IMPL::TrackerRawDataImpl * status) {
  
  ShortVec::iterator iter = status->adcValues().begin();
  while ( iter != status->adcValues().end() ) {
    if ( *iter == EUTELESCOPE::HITPIXEL ) {
      *iter = EUTELESCOPE::GOODPIXEL;
    } else  if ( *iter == EUTELESCOPE::MISSINGPIXEL ) {
      *iter = EUTELESCOPE::GOODPIXEL;
    }
    ++iter; 
  }

}

#ifdef MARLIN_USE_AIDA
void EUTelClusteringProcessor::fillHistos (LCEvent * evt) {

  EUTelEventImpl * eutelEvent = static_cast<EUTelEventImpl*> (evt);
  EventType type              = eutelEvent->getEventType();
  
  if ( type == kEORE ) {
    streamlog_out ( DEBUG4 ) << "EORE found: nothing else to do." << endl;
    return ;
  } else if ( type == kUNKNOWN ) {
    streamlog_out ( WARNING2 )  << "Event number " << evt->getEventNumber() 
				<< " is of unknown type. Continue considering it as a normal Data Event." << endl;
  }
  
  try {

    LCCollectionVec * pulseCollectionVec = dynamic_cast<LCCollectionVec*>  (evt->getCollection(_pulseCollectionName));
    CellIDDecoder<TrackerPulseImpl > cellDecoder(pulseCollectionVec);
    
    // I also need the noise collection too fill in the SNR histograms
    LCCollectionVec * noiseCollectionVec    = dynamic_cast < LCCollectionVec * > (evt->getCollection(_noiseCollectionName));
    LCCollectionVec * statusCollectionVec   = dynamic_cast < LCCollectionVec * > (evt->getCollection(_statusCollectionName));
    CellIDDecoder<TrackerDataImpl > noiseDecoder(noiseCollectionVec);
    
    vector<unsigned short> eventCounterVec( _noOfDetector, 0 );
    
    for ( int iPulse = 0; iPulse < pulseCollectionVec->getNumberOfElements(); iPulse++ ) {
      TrackerPulseImpl * pulse = dynamic_cast<TrackerPulseImpl*> ( pulseCollectionVec->getElementAt(iPulse) );
      ClusterType        type  = static_cast<ClusterType> ( static_cast<int> ( cellDecoder(pulse)["type"] ));
      SparsePixelType    pixelType = static_cast<SparsePixelType> (0);
      EUTelVirtualCluster * cluster;
      
      if ( type == kEUTelFFClusterImpl ) {
	cluster = new EUTelFFClusterImpl ( static_cast<TrackerDataImpl*> ( pulse->getTrackerData() ) );
      } else if ( type == kEUTelSparseClusterImpl ) {
	// knowing that is a sparse cluster is not enough we need also
	// to know also the sparse pixel type. This information is
	// available in the "original_zsdata" collection. Let's get it! 
	LCCollectionVec * sparseClusterCollectionVec = dynamic_cast < LCCollectionVec * > (evt->getCollection("original_zsdata"));
	TrackerDataImpl * oneCluster = dynamic_cast<TrackerDataImpl*> (sparseClusterCollectionVec->getElementAt( 0 ));
	CellIDDecoder<TrackerDataImpl > anotherDecoder(sparseClusterCollectionVec);
	pixelType = static_cast<SparsePixelType> ( static_cast<int> ( anotherDecoder( oneCluster )["sparsePixelType"] ));
	if ( pixelType == kEUTelSimpleSparsePixel ) {
	  cluster = new EUTelSparseClusterImpl<EUTelSimpleSparsePixel > ( static_cast<TrackerDataImpl*> ( pulse->getTrackerData() ) );
	} else {
	  streamlog_out ( ERROR4 ) <<  "Unknown cluster type. Sorry for quitting" << endl;
	  throw UnknownDataTypeException("Cluster type unknown");
	}
      } else {
	streamlog_out ( ERROR4 ) <<  "Unknown cluster type. Sorry for quitting" << endl;
	throw UnknownDataTypeException("Cluster type unknown");
      }
      
      int detectorID = cluster->getDetectorID();
    
      // increment of one unit the event counter for this plane
      eventCounterVec[detectorID]++;
    
      string tempHistoName;

      {
	stringstream ss;
	ss << _clusterSignalHistoName << "-d" << detectorID;
	tempHistoName = ss.str();
      } 
      (dynamic_cast<AIDA::IHistogram1D*> (_aidaHistoMap[tempHistoName]))->fill(cluster->getTotalCharge());
    
      {
	stringstream ss;
	ss << _seedSignalHistoName << "-d" << detectorID;
	tempHistoName = ss.str();
      }
      (dynamic_cast<AIDA::IHistogram1D*> (_aidaHistoMap[tempHistoName]))->fill(cluster->getSeedCharge());
    
      vector<float > charges = cluster->getClusterCharge(_clusterSpectraNVector);
      for ( unsigned int i = 0; i < charges.size() ; i++ ) {
	{
	  stringstream ss;
	  ss << _clusterSignalHistoName << _clusterSpectraNVector[i] << "-d" << detectorID;
	  tempHistoName = ss.str();
	}
	(dynamic_cast<AIDA::IHistogram1D*> (_aidaHistoMap[tempHistoName]))
	  ->fill(charges[i]);
      }
    
      vector<int >::iterator iter = _clusterSpectraNxNVector.begin();
      while ( iter != _clusterSpectraNxNVector.end() ) {
	{
	  stringstream ss;
	  ss << _clusterSignalHistoName << (*iter) << "x" << (*iter) << "-d" << detectorID;
	  tempHistoName = ss.str();
	}
	(dynamic_cast<AIDA::IHistogram1D*> (_aidaHistoMap[tempHistoName]))
	  ->fill(cluster->getClusterCharge((*iter), (*iter)));
	++iter;
      }
    
      {
	stringstream ss;
	ss << _hitMapHistoName << "-d" << detectorID;
	tempHistoName = ss.str();
      } 
      int xSeed, ySeed;
      cluster->getCenterCoord(xSeed, ySeed);
      (dynamic_cast<AIDA::IHistogram2D*> (_aidaHistoMap[tempHistoName]))
	->fill(static_cast<double >(xSeed), static_cast<double >(ySeed), 1.);
    
    
      // fill the noise related histograms 
    
      // get the noise TrackerDataImpl corresponding to the detector
      // under analysis and the status matrix as well
      TrackerDataImpl    * noiseMatrix  = dynamic_cast<TrackerDataImpl *>    (noiseCollectionVec->getElementAt(detectorID) );
      TrackerRawDataImpl * statusMatrix = dynamic_cast<TrackerRawDataImpl *> (statusCollectionVec->getElementAt(detectorID) );
    
      // prepare also a MatrixDecoder for this matrix
      EUTelMatrixDecoder noiseMatrixDecoder(noiseDecoder, noiseMatrix);
    
      vector<float > noiseValues;
      if ( type == kEUTelFFClusterImpl ) {
	for ( int yPixel = ySeed - ( _yClusterSize / 2 ); yPixel <= ySeed + ( _yClusterSize / 2 ); yPixel++ ) {
	  for ( int xPixel = xSeed - ( _xClusterSize / 2 ); xPixel <= xSeed + ( _xClusterSize / 2 ); xPixel++ ) {
	    // always check we are still within the sensor!!!
	    if ( ( xPixel >= _minX[detectorID] )  &&  ( xPixel <= _maxX[detectorID] ) &&
		 ( yPixel >= _minY[detectorID] )  &&  ( yPixel <= _maxY[detectorID] ) ) {
	      int index = noiseMatrixDecoder.getIndexFromXY(xPixel, yPixel);
	      // the corresponding position in the status matrix has to be HITPIXEL
	      bool isHit      = ( statusMatrix->getADCValues()[index] == EUTELESCOPE::HITPIXEL );
	      bool isBad      = ( statusMatrix->getADCValues()[index] == EUTELESCOPE::BADPIXEL );
	      bool isMissing  = ( statusMatrix->getADCValues()[index] == EUTELESCOPE::MISSINGPIXEL );
	      if ( !isMissing && !isBad && isHit ) {
		noiseValues.push_back( noiseMatrix->getChargeValues()[index] );
	      } else {
		noiseValues.push_back( 0. );
	      }
	    } else {
	      noiseValues.push_back( 0. );
	    }
	  }
	}
      } else if ( type == kEUTelSparseClusterImpl ) {
	if ( pixelType == kEUTelSimpleSparsePixel ) {
	  auto_ptr<EUTelSimpleSparsePixel> pixel ( new EUTelSimpleSparsePixel );
	  // this recasting is due to have access to sparse cluster
	  // specific methods.
	  EUTelSparseClusterImpl<EUTelSimpleSparsePixel>* recasted = 
	    dynamic_cast<EUTelSparseClusterImpl<EUTelSimpleSparsePixel>* > (cluster);
	  for ( unsigned int iPixel = 0 ; iPixel < recasted->size() ; iPixel++ ) {
	    recasted->getSparsePixelAt( iPixel , pixel.get() );
	    int index = noiseMatrixDecoder.getIndexFromXY( pixel->getXCoord(), pixel->getYCoord() );
	    noiseValues.push_back( noiseMatrix->getChargeValues() [ index ] );
	    
	  }
	}
      }
    
      bool fillSNRSwitch = true;
      try {
	cluster->setNoiseValues(noiseValues);
      } catch ( IncompatibleDataSetException& e ) {
	streamlog_out ( WARNING2 ) << e.what() << endl
				   << "Continuing without filling the noise histograms" << endl;
	fillSNRSwitch = false;
      }
    
    
      if ( fillSNRSwitch ) {
      
	AIDA::IHistogram1D * histo;
      
	{
	  stringstream ss;
	  ss << _clusterNoiseHistoName << "-d" << detectorID;
	  tempHistoName = ss.str();
	}
	histo = dynamic_cast<AIDA::IHistogram1D* > ( _aidaHistoMap[tempHistoName] );
	if ( histo ) {
	  histo->fill( cluster->getClusterNoise() );
	}
      
      
	{
	  stringstream ss;
	  ss << _clusterSNRHistoName << "-d" << detectorID;
	  tempHistoName = ss.str();
	}
	histo = dynamic_cast<AIDA::IHistogram1D* > ( _aidaHistoMap[tempHistoName] );
	if ( histo ) {
	  histo->fill( cluster->getClusterSNR() );
	}
      
	{
	  stringstream ss;
	  ss << _seedSNRHistoName << "-d" << detectorID;
	  tempHistoName = ss.str();
	}
	histo = dynamic_cast<AIDA::IHistogram1D * > ( _aidaHistoMap[tempHistoName] );
	if ( histo ) {
	  histo->fill( cluster->getSeedSNR() );
	}
      
	vector<int >::iterator iter = _clusterSpectraNxNVector.begin();
	while ( iter != _clusterSpectraNxNVector.end() ) {
	  {
	    stringstream ss;
	    ss << _clusterSNRHistoName << (*iter) << "x" << (*iter) << "-d" << detectorID;
	    tempHistoName = ss.str();
	  }
	  histo = dynamic_cast<AIDA::IHistogram1D*> ( _aidaHistoMap[tempHistoName] ) ;
	  if ( histo ) {
	    histo->fill(cluster->getClusterSNR( (*iter), (*iter) ));
	  }
	  ++iter;
	}
      
	vector<float > snrs = cluster->getClusterSNR(_clusterSpectraNVector);
	for ( unsigned int i = 0; i < snrs.size() ; i++ ) {
	  {
	    stringstream ss;
	    ss << _clusterSNRHistoName << _clusterSpectraNVector[i] << "-d" << detectorID;
	    tempHistoName = ss.str();
	  }
	  histo = dynamic_cast<AIDA::IHistogram1D * > ( _aidaHistoMap[tempHistoName] ) ;
	  if ( histo ) {
	    histo->fill( snrs[i] ); 
	  }
	}
      }
    
      delete cluster;
    }

    // fill the event multiplicity here
    string tempHistoName;
    for ( int iDetector = 0; iDetector < _noOfDetector; iDetector++ ) {
      {
	stringstream ss;
	ss << _eventMultiplicityHistoName << "-d" << iDetector;
	tempHistoName = ss.str();
      }
      AIDA::IHistogram1D * histo = dynamic_cast<AIDA::IHistogram1D *> ( _aidaHistoMap[tempHistoName] );
      if ( histo ) {
	histo->fill( eventCounterVec[iDetector] );
      }
    }
  } catch (lcio::DataNotAvailableException& e) {
    return;
  }
  
}

#endif 

#ifdef MARLIN_USE_AIDA
void EUTelClusteringProcessor::bookHistos() {

  // histograms are grouped in loops and detectors
  streamlog_out ( MESSAGE0 )  << "Booking histograms " << endl;
  auto_ptr<EUTelHistogramManager> histoMgr( new EUTelHistogramManager( _histoInfoFileName ));
  EUTelHistogramInfo    * histoInfo;
  bool                    isHistoManagerAvailable;

  try { 
    isHistoManagerAvailable = histoMgr->init();
  } catch ( ios::failure& e) {
    streamlog_out ( WARNING2 ) << "I/O problem with " << _histoInfoFileName << "\n"
			       << "Continuing without histogram manager"  << endl;
    isHistoManagerAvailable = false;
  } catch ( ParseException& e ) {
    streamlog_out ( WARNING2 ) << e.what() << "\n"
			       << "Continuing without histogram manager" << endl;
    isHistoManagerAvailable = false;
  }
  

  string tempHistoName;
  string basePath;
  for (int iDetector = 0; iDetector < _noOfDetector; iDetector++) {
    
    {
      stringstream ss;
      ss << "detector-" << iDetector;
      basePath = ss.str();
    }
    AIDAProcessor::tree(this)->mkdir(basePath.c_str());
    basePath.append("/");

    int    clusterNBin  = 1000;
    double clusterMin   = 0.;
    double clusterMax   = 1000.;
    string clusterTitle = "Cluster spectrum with all pixels";
    if ( isHistoManagerAvailable ) {
      histoInfo = histoMgr->getHistogramInfo( _clusterSignalHistoName );
      if ( histoInfo ) {
	streamlog_out ( DEBUG2 ) << (* histoInfo ) << endl;
	clusterNBin = histoInfo->_xBin;
	clusterMin  = histoInfo->_xMin;
	clusterMax  = histoInfo->_xMax;
	if ( histoInfo->_title != "" ) clusterTitle = histoInfo->_title;
      }
    }      

    int    clusterSNRNBin  = 300;
    double clusterSNRMin   = 0.;
    double clusterSNRMax   = 200;
    string clusterSNRTitle = "Cluster SNR";
    if ( isHistoManagerAvailable ) {
      histoInfo = histoMgr->getHistogramInfo( _clusterSNRHistoName );
      if ( histoInfo ) {
	streamlog_out ( DEBUG2 ) << (* histoInfo ) << endl;
	clusterSNRNBin = histoInfo->_xBin;
	clusterSNRMin  = histoInfo->_xMin;
	clusterSNRMax  = histoInfo->_xMax;
	if ( histoInfo->_title != "" ) clusterSNRTitle = histoInfo->_title;
      }
    }

    // cluster signal
    {
      stringstream ss;
      ss << _clusterSignalHistoName << "-d" << iDetector;
      tempHistoName = ss.str();
    } 
    AIDA::IHistogram1D * clusterSignalHisto = 
      AIDAProcessor::histogramFactory(this)->createHistogram1D( (basePath + tempHistoName).c_str(), 
								clusterNBin,clusterMin,clusterMax);
    _aidaHistoMap.insert(make_pair(tempHistoName, clusterSignalHisto));
    clusterSignalHisto->setTitle(clusterTitle.c_str());

    // cluster SNR
    {
      stringstream ss;
      ss << _clusterSNRHistoName << "-d" << iDetector;
      tempHistoName = ss.str();
    }
    AIDA::IHistogram1D * clusterSNRHisto = 
      AIDAProcessor::histogramFactory(this)->createHistogram1D( (basePath + tempHistoName).c_str(),
								clusterSNRNBin, clusterSNRMin, clusterSNRMax);
    _aidaHistoMap.insert( make_pair(tempHistoName, clusterSNRHisto) ) ;
    clusterSNRHisto->setTitle(clusterSNRTitle.c_str());

    
    vector<int >::iterator iter = _clusterSpectraNVector.begin();
    while ( iter != _clusterSpectraNVector.end() ) {
      // this is for the signal
      {
	stringstream ss;
	ss << _clusterSignalHistoName << (*iter) << "-d" << iDetector;
	tempHistoName = ss.str();
      }
      AIDA::IHistogram1D * clusterSignalNHisto = 
	AIDAProcessor::histogramFactory(this)->createHistogram1D( (basePath + tempHistoName).c_str(),
								  clusterNBin, clusterMin, clusterMax);
      _aidaHistoMap.insert(make_pair(tempHistoName, clusterSignalNHisto) );
      string tempTitle;
      {
	stringstream ss;
	ss << "Cluster spectrum with the " << (*iter) << " most significant pixels ";
	tempTitle = ss.str();
      }
      clusterSignalNHisto->setTitle(tempTitle.c_str());


      // this is for the SNR
      {
	stringstream ss;
	ss << _clusterSNRHistoName << (*iter) << "-d" << iDetector;
	tempHistoName = ss.str();
      }
      AIDA::IHistogram1D * clusterSNRNHisto = 
	AIDAProcessor::histogramFactory(this)->createHistogram1D( (basePath + tempHistoName).c_str(),
								  clusterSNRNBin, clusterSNRMin, clusterSNRMax);
      {
	stringstream ss;
	ss << "Cluster SNR with the " << (*iter) << " most significant pixels";
	tempTitle = ss.str();
      } 
      _aidaHistoMap.insert( make_pair( tempHistoName, clusterSNRNHisto ) );
      clusterSNRNHisto->setTitle(tempTitle.c_str());

      ++iter;
    }

    iter = _clusterSpectraNxNVector.begin();
    while ( iter != _clusterSpectraNxNVector.end() ) {

      // first the signal
      {
	stringstream ss;
	ss << _clusterSignalHistoName << (*iter) << "x" << (*iter) << "-d" << iDetector;
	tempHistoName = ss.str();
      }
      AIDA::IHistogram1D * clusterSignalNxNHisto = 
	AIDAProcessor::histogramFactory(this)->createHistogram1D( (basePath + tempHistoName).c_str(),
								  clusterNBin, clusterMin, clusterMax);
      _aidaHistoMap.insert(make_pair(tempHistoName, clusterSignalNxNHisto) );
      string tempTitle;
      {
	stringstream ss;
	ss << "Cluster spectrum with " << (*iter) << " by " << (*iter) << " pixels ";
	tempTitle = ss.str();
      }
      clusterSignalNxNHisto->setTitle(tempTitle.c_str());

      // then the SNR
      {
	stringstream ss;
	ss << _clusterSNRHistoName << (*iter) << "x" << (*iter) << "-d" << iDetector;
	tempHistoName = ss.str();
      }
      AIDA::IHistogram1D * clusterSNRNxNHisto =
	AIDAProcessor::histogramFactory(this)->createHistogram1D( (basePath + tempHistoName).c_str(),
								  clusterSNRNBin, clusterSNRMin, clusterSNRMax);
      _aidaHistoMap.insert(make_pair(tempHistoName, clusterSNRNxNHisto) );
      {
	stringstream ss;
	ss << "SNR with " << (*iter) << " by " << (*iter) << " pixels ";
	tempTitle = ss.str();
      }
      clusterSNRNxNHisto->setTitle(tempTitle.c_str());


      ++iter;
    }

    {
      stringstream ss;
      ss << _seedSignalHistoName << "-d" << iDetector;
      tempHistoName = ss.str();
    }
    int    seedNBin  = 500;
    double seedMin   = 0.;
    double seedMax   = 500.;
    string seedTitle = "Seed pixel spectrum"; 
    if ( isHistoManagerAvailable ) {
      histoInfo = histoMgr->getHistogramInfo( _seedSignalHistoName );
      if ( histoInfo ) {
	streamlog_out ( DEBUG2 ) << (* histoInfo ) << endl;
	seedNBin = histoInfo->_xBin;
	seedMin  = histoInfo->_xMin;
	seedMax  = histoInfo->_xMax;
	if ( histoInfo->_title != "" ) seedTitle = histoInfo->_title;
      }
    }
    AIDA::IHistogram1D * seedSignalHisto =
      AIDAProcessor::histogramFactory(this)->createHistogram1D( (basePath + tempHistoName).c_str(),
								seedNBin, seedMin, seedMax);
    _aidaHistoMap.insert(make_pair(tempHistoName, seedSignalHisto));
    seedSignalHisto->setTitle(seedTitle.c_str());
  
    {
      stringstream ss;
      ss << _seedSNRHistoName << "-d" << iDetector;
      tempHistoName = ss.str();
    }
    int    seedSNRNBin  =  300;
    double seedSNRMin   =    0.;
    double seedSNRMax   =  200.;
    string seedSNRTitle = "Seed SNR";
    if ( isHistoManagerAvailable ) {
      histoInfo = histoMgr->getHistogramInfo( _seedSNRHistoName );
      if ( histoInfo ) {
	streamlog_out ( DEBUG2 ) << (* histoInfo ) << endl;
	seedSNRNBin = histoInfo->_xBin;
	seedSNRMin  = histoInfo->_xMin;
	seedSNRMax  = histoInfo->_xMax;
	if ( histoInfo->_title != "" ) seedSNRTitle = histoInfo->_title;
      }
    }
    AIDA::IHistogram1D * seedSNRHisto = 
      AIDAProcessor::histogramFactory(this)->createHistogram1D( (basePath + tempHistoName).c_str(),
								seedSNRNBin, seedSNRMin, seedSNRMax);
    _aidaHistoMap.insert( make_pair(tempHistoName, seedSNRHisto ));
    seedSNRHisto->setTitle(seedSNRTitle.c_str());


    {
      stringstream ss;
      ss << _clusterNoiseHistoName << "-d" << iDetector;
      tempHistoName = ss.str();
    }
    int    clusterNoiseNBin  =  300;
    double clusterNoiseMin   =    0.;
    double clusterNoiseMax   =  200.;
    string clusterNoiseTitle = "Cluster noise";
    if ( isHistoManagerAvailable ) {
      histoInfo = histoMgr->getHistogramInfo( _clusterNoiseHistoName );
      if ( histoInfo ) {
	streamlog_out ( DEBUG2 ) << (* histoInfo ) << endl;
	clusterNoiseNBin = histoInfo->_xBin;
	clusterNoiseMin  = histoInfo->_xMin;
	clusterNoiseMax  = histoInfo->_xMax;
	if ( histoInfo->_title != "" ) clusterNoiseTitle = histoInfo->_title;
      }
    }
    AIDA::IHistogram1D * clusterNoiseHisto = 
      AIDAProcessor::histogramFactory(this)->createHistogram1D( (basePath + tempHistoName).c_str(),
								clusterNoiseNBin, clusterNoiseMin, clusterNoiseMax);
    _aidaHistoMap.insert( make_pair(tempHistoName, clusterNoiseHisto ));
    clusterNoiseHisto->setTitle(clusterNoiseTitle.c_str());


    {
      stringstream ss;
      ss << _hitMapHistoName << "-d" << iDetector;
      tempHistoName = ss.str();
    } 
    int     xBin = _maxX[iDetector] - _minX[iDetector] + 1;
    double  xMin = static_cast<double >(_minX[iDetector]) - 0.5;
    double  xMax = static_cast<double >(_maxX[iDetector]) + 0.5;
    int     yBin = _maxY[iDetector] - _minY[iDetector] + 1;
    double  yMin = static_cast<double >(_minY[iDetector]) - 0.5;
    double  yMax = static_cast<double >(_maxY[iDetector]) + 0.5;
    AIDA::IHistogram2D * hitMapHisto = 
      AIDAProcessor::histogramFactory(this)->createHistogram2D( (basePath + tempHistoName).c_str(),
								xBin, xMin, xMax,yBin, yMin, yMax);
    _aidaHistoMap.insert(make_pair(tempHistoName, hitMapHisto));
    hitMapHisto->setTitle("Hit map");

    {
      stringstream ss;
      ss << _eventMultiplicityHistoName << "-d" << iDetector;
      tempHistoName = ss.str();
    }
    int     eventMultiNBin  = 30;
    double  eventMultiMin   =  0.;
    double  eventMultiMax   = 30.;
    string  eventMultiTitle = "Event multiplicity";
    if ( isHistoManagerAvailable ) {
      histoInfo = histoMgr->getHistogramInfo(  _eventMultiplicityHistoName );
      if ( histoInfo ) {
	streamlog_out ( DEBUG2 ) << (* histoInfo ) << endl;
	eventMultiNBin  = histoInfo->_xBin;
	eventMultiMin   = histoInfo->_xMin;
	eventMultiMax   = histoInfo->_xMax;
	if ( histoInfo->_title != "" ) eventMultiTitle = histoInfo->_title;
      }
    }
    AIDA::IHistogram1D * eventMultiHisto =
      AIDAProcessor::histogramFactory(this)->createHistogram1D( (basePath + tempHistoName).c_str(),
								eventMultiNBin, eventMultiMin, eventMultiMax);
    _aidaHistoMap.insert( make_pair(tempHistoName, eventMultiHisto) );
    eventMultiHisto->setTitle( eventMultiTitle.c_str() );

  }
  
}
#endif
