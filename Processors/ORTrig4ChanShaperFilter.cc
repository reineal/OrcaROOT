// ORTrig4ChanShaperFilter.cc

#include "ORTrig4ChanShaperFilter.hh"
#include "ORLogger.hh"

using namespace std;


ORTrig4ChanShaperFilter::ORTrig4ChanShaperFilter()
{
  SetComponentBreakReturnsFailure();
  
  fTimeCutLength = 0.0005;// in units of secs
  f64PDHistDrawer = new OR64PDHistDrawer(&fShaperDecoder);
  AddProcessor(f64PDHistDrawer);


  fShaperTreeWriter = new ORBasicTreeWriter(&fShaperDecoder, "shaperFilterTree");
  AddProcessor(fShaperTreeWriter);
  
  fTriggerTreeWriter = new ORTrig4ChanTreeWriter("triggerFilterTree");
  AddProcessor(fTriggerTreeWriter);
  
  fLastTriggerRecordPtr = NULL;
  fTempShaperRecordPtr = NULL;
   Reset = 0;
}

ORTrig4ChanShaperFilter::~ORTrig4ChanShaperFilter()
{
  if(fLastTriggerRecordPtr != NULL) {
    delete[] fLastTriggerRecordPtr;
  }
  if(fTempShaperRecordPtr != NULL) {
    delete[] fTempShaperRecordPtr;
  }
  
  delete f64PDHistDrawer;
  delete fShaperTreeWriter;
  delete fTriggerTreeWriter;
}
ORDataProcessor::EReturnCode ORTrig4ChanShaperFilter::StartRun()
{
  fShaperDataId = fShaperTreeWriter->GetDataId();
  fTriggerDataId = fTriggerTreeWriter->GetDataId();
  
  return ORCompoundDataProcessor::StartRun();
}
ORDataProcessor::EReturnCode ORTrig4ChanShaperFilter::ProcessDataRecord(UInt_t* record)
{
  UInt_t thisDataId = fShaperDecoder.DataIdOf(record);
 
  if( thisDataId == fShaperDataId ) {
     Reset++;
    if (fTempShaperRecordPtr==NULL) { 
       fShaperRecordLength = fShaperDecoder.LengthOf(record);
    }
    UInt_t tagNoise = 0;
    fThisTriggerTime = (Double_t)fTriggerDecoder.ClockOf(fLastTriggerRecordPtr)/50000000.0;
    fThisCard = fShaperDecoder.CardOf(record);  
    fThisChannel = fShaperDecoder.ChannelOf(record);
    if ( fBothRecords.size() > 0 ) {
      //iterator to tag records old enough to delete
      multimap<Double_t, UInt_t*>::iterator del_It= fBothRecords.begin();

      for (multimap<Double_t, UInt_t*>::iterator it = fBothRecords.begin();
         it != fBothRecords.end();
         ++it) {
        fMapTriggerTime = it->first;
        if ( fThisTriggerTime - fMapTriggerTime > fTimeCutLength ) {
           //done with old records
            delete[] it->second;
            it->second = NULL;
            //save iterator location of last record to erase 
            del_It++;
         } else { //within time cut
          fLastCard = fShaperDecoder.CardOf(it->second);  
          fLastChannel = fShaperDecoder.ChannelOf(it->second);
          if ( fThisTriggerTime >= fMapTriggerTime ) {
             //normal order events
             if ( fThisCard == fLastCard ) {
                if ( fThisChannel == fLastChannel ) {
                   tagNoise = 1;//don't print current record
                }
             }
          } else { 
             //correct for out of order events
             ORLog(kDebug) << "Cut Triggers coming out of order!" << endl;
             ORLog(kDebug) << "Processing record " << fThisTriggerTime  << endl;
             ORLog(kDebug) << "Map record Time " << fMapTriggerTime << endl;
             ORLog(kDebug) << "Difference " << fThisTriggerTime - fMapTriggerTime << endl;
             ORLog(kDebug) << "Reset " << Reset << endl;
             if (fMapTriggerTime - fThisTriggerTime > fTimeCutLength ) {
                if ( fThisCard == fLastCard ) {
                   if ( fThisChannel == fLastChannel ) {
                      tagNoise = 1;//don't print current record
                   }
                }
             }
             
          }
        }
      }
      //records are automatically sorted by multimap, erase everything up to del_It
      fBothRecords.erase(fBothRecords.begin(), del_It);
    }
    if ( tagNoise == 0 ) {
      if (!fDoProcess || !fDoProcessRun) return kFailure;
      //add time to hist BEFORE adding shaper record
      EReturnCode retCode = f64PDHistDrawer->ProcessRecordTime(fThisTriggerTime);
      if (retCode == kBreak) return fBreakRetCode;
      if (retCode >= kAlarm) return retCode;      
      //add this shaper record to hist
      retCode = f64PDHistDrawer->ProcessDataRecord(record); 
      if (retCode == kBreak) return fBreakRetCode;
      if (retCode >= kAlarm) return retCode;      
      retCode = fShaperTreeWriter->ProcessDataRecord(record); 
      if (retCode == kBreak) return fBreakRetCode;
      if (retCode >= kAlarm) return retCode;      
      retCode = fTriggerTreeWriter->ProcessDataRecord(fLastTriggerRecordPtr); 
      if (retCode == kBreak) return fBreakRetCode;
      if (retCode >= kAlarm) return retCode;      
    }
    //now add this record to fBothRecords
    fTempShaperRecordPtr = new UInt_t[fShaperRecordLength]; 
    memcpy(fTempShaperRecordPtr, record, fShaperRecordLength*sizeof(UInt_t));
    fBothRecords.insert(pair<Double_t, UInt_t*>(fThisTriggerTime, fTempShaperRecordPtr) );
    fLastRecordDataId = thisDataId;  
  } else if( thisDataId == fTriggerDataId ) {
     Reset = 0;
    if (fLastTriggerRecordPtr==NULL) { 
      /* if not allocated, reallocate a buffer and then copy. */
      fTriggerRecordLength = fShaperDecoder.LengthOf(record);
      fLastTriggerRecordPtr = new UInt_t[fTriggerRecordLength];  
       ORLog(kDebug) << "Trigger Length" << fTriggerRecordLength << endl;
    }
    /* copy */
    memcpy(fLastTriggerRecordPtr, record, fTriggerRecordLength*sizeof(UInt_t));
    fLastRecordDataId = thisDataId;  
  } 
  
  return kSuccess;

}
ORDataProcessor::EReturnCode ORTrig4ChanShaperFilter::EndRun()
{
  if ( fBothRecords.size() > 0 ) {
    for (multimap<Double_t, UInt_t*>::iterator it = fBothRecords.begin();
        it != fBothRecords.end();
        ++it) {
      if( it->second != NULL ){
        delete[] it->second;        
        it->second = NULL;
      }
    }
    fBothRecords.clear();
  }
  
  
  if(fLastTriggerRecordPtr != NULL) {
    delete[] fLastTriggerRecordPtr;
    fLastTriggerRecordPtr = NULL;
  }
  if(fTempShaperRecordPtr != NULL) {
    //delete[] fTempShaperRecordPtr;//deleted by map
    fTempShaperRecordPtr = NULL;
  }
  
  return ORCompoundDataProcessor::EndRun();
}
ORDataProcessor::EReturnCode ORTrig4ChanShaperFilter::EndProcessing()
{
  if ( fBothRecords.size() > 0 ) {
    for (multimap<Double_t, UInt_t*>::iterator it = fBothRecords.begin();
      it != fBothRecords.end();
      ++it) {
      if( it->second != NULL ){
        delete[] it->second;        
        it->second = NULL;
      }
    }
    fBothRecords.clear();
  }
  return ORCompoundDataProcessor::EndProcessing();
}

