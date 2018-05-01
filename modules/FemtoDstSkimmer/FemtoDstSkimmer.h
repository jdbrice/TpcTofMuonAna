#ifndef FEMTO_DST_SKIMMER_H
#define FEMTO_DST_SKIMMER_H

#include "TreeAnalyzer.h"
#include "HistoBins.h"

// FemtoDstFormat
#include "FemtoDstFormat/BranchReader.h"
#include "FemtoDstFormat/TClonesArrayReader.h"
#include "FemtoDstFormat/FemtoEvent.h"
#include "FemtoDstFormat/FemtoTrack.h"
#include "FemtoDstFormat/FemtoMcTrack.h"
#include "FemtoDstFormat/FemtoTrackHelix.h"
#include "FemtoDstFormat/FemtoBTofPidTraits.h"
#include "FemtoDstFormat/FemtoTrackProxy.h"

// Analyzers
// #include "FemtoDstSkimmer/PairHistogramMaker.h"
#include "FemtoDstSkimmer/TrackHistogramMaker.h"
// #include "FemtoDstSkimmer/MtdHistogramMaker.h"

#include "Filters/TrackFilter.h"

#include "FemtoDstSkimmer/TofGenerator.h"
#include "ZRC/ZbRC.h"

#include <map>

#define LOGURU_WITH_STREAMS 1
#include "vendor/loguru.h"

class FemtoDstSkimmer : public TreeAnalyzer
{
protected:
	FemtoEvent *_event;

	BranchReader<FemtoEvent> _rEvent;
	TClonesArrayReader<FemtoTrack> _rTracks;
	TClonesArrayReader<FemtoTrackHelix> _rHelices;
	TClonesArrayReader<FemtoBTofPidTraits> _rBTofPid;

	TrackHistogramMaker thm;
	bool trackQA = false;
	TrackFilter _trackFilter;

	HistoBins hbP, hbEta, hbCen;
	map<int, int> cMap;

	TofGenerator tofGen;

	double rTof( double zb, double p, double m ){
		double zbp = zb - tofGen.mean( p, m );
		return zbp;
	}

	ZbRC zbUtil;


public:
	virtual const char* classname() const {return "FemtoDstSkimmer";}
	FemtoDstSkimmer() : zbUtil(0.014) {}
	~FemtoDstSkimmer() {}

	virtual void initialize(){
		TreeAnalyzer::initialize();

		_rEvent.setup( chain, "Event" );
		_rTracks.setup( chain, "Tracks" );
		_rHelices.setup( chain, "Helices" );
		_rBTofPid.setup( chain, "BTofPidTraits" );


		trackQA = config.getBool( "TrackHistogramMaker:enable", true );
		if ( trackQA ) {
			LOG_F( INFO, "Creating Track QA" );
			thm.setup( config, "TrackHistogramMaker", book );
		}
		_trackFilter.load( config, nodePath + ".TrackFilter" );
		book->cd();


		hbP.load( config, "bins.signal.mP" );
		hbEta.load( config, "bins.signal.mEta" );
		hbCen.load( config, "bins.signal.mCen" );

		cMap = config.getIntMap( nodePath + ".CentralityMap" );
		for ( int i = 0; i < 16; i++ )
			LOG_F( INFO, "cMap[%d] = %d", i, cMap[i] );

		zbUtil = config.get<ZbRC>( "ZbRC" );


	}

	static string charge_string( int c ){
		if ( c < 0 )
			return "n";
		if ( c > 0 )
			return "p";
		return "E";
	}
	static string bin_name( int cen, int eta, int c ){
		stringstream sstr;
		sstr << "cen_" << cen << "_eta_" << eta << "_c_" << charge_string( c );
		return sstr.str(); 
	}


protected:

	virtual void preEventLoop(){
		TreeAnalyzer::preEventLoop();

		book->cd();

	}
	virtual void analyzeEvent(){
	
		_event = _rEvent.get();
		book->fill( "Events", 1 );

	
		size_t nTracks = _rTracks.N();
		FemtoTrackProxy _proxy;


		for (size_t i = 0; i < nTracks; i++ ){
			_proxy.assemble( i, _rTracks, _rHelices, _rBTofPid );

			if ( nullptr == _proxy._btofPid ) continue;

			if ( _trackFilter.fail( _proxy ) ) continue;


			if ( fabs(_proxy._btofPid->yLocal()) > 1.6 ) continue;
			if ( fabs(_proxy._btofPid->zLocal()) > 2.8 ) continue;

			double p = _proxy._track->mPt * cosh( _proxy._track->mEta );
			double lzb = rTof( 1.0/_proxy._btofPid->beta(), p, 0.105658374 );


			int pBin = hbP.findBin( p );
			if ( pBin < 0 ) continue;
			double avgP = hbP.bins[ pBin ] + hbP.binWidth( pBin ) / 2.0;

			// LOG_F(  INFO, "p = %f, pBin = %d", p, pBin);
			double zb = zbUtil.nlTof( "mu", _proxy._btofPid->beta(), p, avgP );
			// LOG_F( INFO, "p=%f, avgP{bin}=%f", p, avgP );

			book->fill( "zd_p", p, _proxy._track->nSigmaPion() );
			book->fill( "zb_p", p, zb );

			book->fill( "lzb_p", p, lzb );

			book->fill( "yLocal_p", p, _proxy._btofPid->yLocal() );
			book->fill( "zLocal_p", p, _proxy._btofPid->zLocal() );

		}
	}


	virtual void postEventLoop(){
		TreeAnalyzer::postEventLoop();

		if ( 0 == config.getInt( "jobIndex" ) || -1 == config.getInt( "jobIndex" ) ){
			TNamed config_str( "config", config.toXml() );
			config_str.Write();
		}
		
	}
	
};

#endif
