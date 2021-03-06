/*
  Template of calibration/filtering macro using ESD:
  - requires AliESDs.root and AliESDfriend.root
  - requires OCDB access (default set to "raw://")
  - requires run number as argument to init OCDB
  - calls LoadLibraries.C, ConfigCalibTrain.C and AddTaskTPCCalib.C macros
  - output CalibObjects.root with TPC and TRD calibration objects are created

  Example:
  .L $ALICE_PHYSICS/ANALYSIS/macros/runCalibTrain.C
  runCalibTrain("104892");
*/

void main_runCalibTrain(Int_t runNumber, const char* inFileName = "AliESDs.root", const char* ocdb="raw://")
{
  //
  // macro to run TPC calibration train 
  //
  TStopwatch sw;
  sw.Start();
  AliSysInfo::SetVerbose(kTRUE);
  AliLog::SetGlobalLogLevel(AliLog::kError); 

  // switch off debug 
  AliLog::SetClassDebugLevel("AliESDEvent",0);
  
  // steering input chain
  TChain *chain = new TChain("esdTree");
  chain->Add(inFileName);

  // config calibration train
  // setting geometry and B-field from GRP
  printf("runNumber from runCalibTrain = %d\n",runNumber);
  printf("ocdb from runCalibTrain = %s\n",ocdb);
  if (gSystem->AccessPathName("OCDB.root", kFileExists)==0) {        
    AliCDBManager::Instance()->SetSnapshotMode("OCDB.root");
    printf("ocdb from snapshot\n");
  }

  AliSysInfo::AddStamp("BeforeConfiguringCalibTrain");
  ConfigCalibTrain(runNumber, ocdb);
  AliSysInfo::AddStamp("AfterConfiguringCalibTrain");  

  if (gSystem->AccessPathName("localOCDBaccessConfig.C", kFileExists)==0) {
    gInterpreter->ProcessLine("localOCDBaccessConfig();");
  }
  
  //
  // check the presence of the detectors
  AliCDBEntry* entry = AliCDBManager::Instance()->Get("GRP/GRP/Data");
  AliGRPObject* grpData = dynamic_cast<AliGRPObject*>(entry->GetObject()); 
  if (!grpData) {printf("Failed to get GRP data for run %d", runNumber); return;}
  Int_t activeDetectors = grpData->GetDetectorMask(); 
  TString detStr = AliDAQ::ListOfTriggeredDetectors(activeDetectors);
  printf("Detectors in the data:\n%s\n",detStr.Data());

  // setup analysis
  //
  AliAnalysisManager *mgr  = new AliAnalysisManager("ESD to ESD", "Analysis Manager");
  // mgr->SetDebugLevel(3);
  mgr->SetNSysInfo(50);   
  mgr->SetCacheSize(0);

  // Input
  AliESDInputHandler* inpHandler = new AliESDInputHandler();
  inpHandler->SetReadFriends(1);
  mgr->SetInputEventHandler(inpHandler);
  
  // Output
  const char *outFile = "CalibObjects.root";
  AliESDHandler* esdHandler   = new AliESDHandler();
  mgr->SetOutputEventHandler(esdHandler);
  esdHandler->SetOutputFileName(outFile);
  mgr->SetCommonFileName(outFile);
  //  
  // Detector Tasks
  //
  AliSysInfo::AddStamp("BeforeTPC");
  //if ( detStr.Contains("TPC"))    AddTaskTPCCalib(runNumber);
  if ( detStr.Contains("TPC"))    AddTaskTPCCalib();

  AliSysInfo::AddStamp("BeforeTRD");
  if ( detStr.Contains("TRD") && detStr.Contains("TPC"))    AddTaskTRDCalib(runNumber);

  AliSysInfo::AddStamp("BeforeT0");
  if ( detStr.Contains("T0"))     AddTaskT0Calib(runNumber);

  AliSysInfo::AddStamp("BeforeMeanVertex");
  if ( detStr.Contains("ITSSPD")) AddTaskMeanVertexCalib();


  //
  Int_t doSDD=0;
  if(gSystem->Getenv("ALIEN_JDL_CPASS0_SDD_CALIBRATION")){
    doSDD=atoi(gSystem->Getenv("ALIEN_JDL_CPASS0_SDD_CALIBRATION"));
  }
  if(doSDD>0){
    Bool_t okTPC = detStr.Contains("TPC");
    Bool_t useTPCcrv=kTRUE;
    Bool_t writeITSTP = kFALSE;
    if (!okTPC) useTPCcrv = kFALSE;
    AliSysInfo::AddStamp("BeforeSDD");
    AliAnalysisTaskITSAlignQA *itsAlign = AddTaskSDDCalib(0,writeITSTP,useTPCcrv, detStr.Contains("TOF") ? 20.0:-1);
    if (!okTPC) itsAlign->SetUseITSstandaloneTracks(kTRUE); 
    if (grpData->GetL3Current()[0] < 300) itsAlign->SetMinPt(0.001);
  }

  // Make sure the TOF is the last one since it modifies the ESDevent 
  AliSysInfo::AddStamp("BeforeTOF");
  if ( detStr.Contains("TOF") && detStr.Contains("TPC"))    AddTOFAnalysisTaskCalibPass0();

  //
  // dummy task to clean geometry in Terminate >>>>
  //TMacro cleanGeomMacro("CleanGeom.C");
  CleanGeom* clgmTask = new CleanGeom("cleanGeom");
  //CleanGeom* clgmTask = reinterpret_cast<CleanGeom*>(cleanGeomMacro.Exec("cleanGeom"));
  mgr->AddTask(clgmTask);
  AliAnalysisDataContainer *dummyInp = mgr->GetCommonInputContainer();
  if (dummyInp) mgr->ConnectInput(clgmTask,0,dummyInp);

  // Run the analysis
  AliSysInfo::AddStamp("BeforeInitAnalysis");
  if (!mgr->InitAnalysis()) {
    printf("Analysis cannot be started, returning\n");
    return;
  }
  
  mgr->PrintStatus();
  AliSysInfo::AddStamp("BeforeStartAnalysis");
  sw.Stop();
  printf("runCalibTrain: Config time: "); sw.Print();
  sw.Start(kTRUE);
  mgr->StartAnalysis("local", chain);
  sw.Stop();
  printf("runCalibTrain: Processing time: "); sw.Print();
  return;
}
