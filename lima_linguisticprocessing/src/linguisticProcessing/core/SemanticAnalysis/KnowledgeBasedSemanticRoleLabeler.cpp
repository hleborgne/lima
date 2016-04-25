/*
    Copyright 2016 CEA LIST

    This file is part of LIMA.

    LIMA is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    LIMA is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with LIMA.  If not, see <http://www.gnu.org/licenses/>
*/

#include "KnowledgeBasedSemanticRoleLabeler.h"

#include "common/Data/LimaString.h"
#include "common/misc/Exceptions.h"
#include "common/Data/strwstrtools.h"
#include "common/AbstractFactoryPattern/SimpleFactory.h"
#include "linguisticProcessing/core/AnalysisDumpers/ConllDumper.h"
#include "linguisticProcessing/core/LinguisticResources/LinguisticResources.h"
#include "linguisticProcessing/core/LinguisticProcessors/LimaStringText.h"
#include "linguisticProcessing/core/LinguisticProcessors/LinguisticMetaData.h"
#include "linguisticProcessing/core/LinguisticAnalysisStructure/AnalysisGraph.h"
#include "common/XMLConfigurationFiles/xmlConfigurationFileExceptions.h"
#include "common/tools/FileUtils.h"
#include "common/MediaticData/mediaticData.h"
#include "common/time/timeUtilsController.h"

#include <string>
#include <Python.h>
#include <QtCore/QTemporaryFile>

using namespace std;
using namespace Lima::LinguisticProcessing::AnalysisDumpers;
using namespace Lima::LinguisticProcessing::LinguisticAnalysisStructure;
using namespace Lima::Common::XMLConfigurationFiles;
using namespace Lima::Common::Misc;

namespace Lima
{
namespace LinguisticProcessing
{
namespace SemanticAnalysis
{

static SimpleFactory<MediaProcessUnit,KnowledgeBasedSemanticRoleLabeler> knowledgeBasedSemanticRoleLabelerFactory(KNOWLEDGEBASEDSEMANTICROLELABELER_CLASSID);


class KnowledgeBasedSemanticRoleLabelerPrivate
{
public:
  KnowledgeBasedSemanticRoleLabelerPrivate();
  virtual ~KnowledgeBasedSemanticRoleLabelerPrivate();

  PyObject* m_instance;
  const MediaProcessUnit* m_dumper;
  const MediaProcessUnit* m_loader;
  QString m_inputSuffix;
  QString m_outputSuffix;
};

KnowledgeBasedSemanticRoleLabelerPrivate::KnowledgeBasedSemanticRoleLabelerPrivate() :
  m_instance(0),
  m_dumper(new ConllDumper())
{}

KnowledgeBasedSemanticRoleLabelerPrivate::~KnowledgeBasedSemanticRoleLabelerPrivate()
{
}

KnowledgeBasedSemanticRoleLabeler::KnowledgeBasedSemanticRoleLabeler() : m_d(new KnowledgeBasedSemanticRoleLabelerPrivate())
{}


KnowledgeBasedSemanticRoleLabeler::~KnowledgeBasedSemanticRoleLabeler()
{
  delete m_d;
}

void KnowledgeBasedSemanticRoleLabeler::init(
  Common::XMLConfigurationFiles::GroupConfigurationStructure& unitConfiguration,
  Manager* manager)

{
#ifdef DEBUG_LP
  SEMANTICANALYSISLOGINIT;
  LDEBUG << "KnowledgeBasedSemanticRoleLabeler::init";
#endif
  
  MediaId language=manager->getInitializationParameters().media;
  try {
    string dumperName=unitConfiguration.getParamsValueAtKey("dumper");
    // create the dumper
    m_d->m_dumper=manager->getObject(dumperName);
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    LERROR << "Missing 'dumper' parameter in KnowledgeBasedSemanticRoleLabeler group for language "
           << (int)language << " !";
    throw InvalidConfiguration();
  }

  try {
    string loaderName=unitConfiguration.getParamsValueAtKey("loader");
    // create the loader
    m_d->m_loader=manager->getObject(loaderName);
  }
  catch (InvalidConfiguration& ) {
    m_d->m_loader = 0;
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    LERROR << "Missing 'loader' parameter in KnowledgeBasedSemanticRoleLabeler group for language "
           << (int)language << " !";
    throw InvalidConfiguration();
  }

  try {
    m_d->m_inputSuffix=QString::fromUtf8(unitConfiguration.getParamsValueAtKey("inputSuffix").c_str());
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    // optional parameter: keep default value
  }

  try {
    m_d->m_outputSuffix=QString::fromUtf8(unitConfiguration.getParamsValueAtKey("outputSuffix").c_str());
  }
  catch (Common::XMLConfigurationFiles::NoSuchParam& ) {
    // optional parameter: keep default value
  }

  QString path;
  QString mode = "VerbNet";
  QString kbsrlLogLevel = "error";
	
  try
  {
    kbsrlLogLevel = QString::fromUtf8(unitConfiguration.getParamsValueAtKey("loglevel").c_str());
  }
  catch (NoSuchParam& )
  {
    // keep default
  }

  try
  {
    path = QString::fromUtf8(unitConfiguration.getParamsValueAtKey("path").c_str());
  }
  catch (NoSuchParam& )
  {
    SEMANTICANALYSISLOGINIT;
    LERROR << "no param 'path' in KnowledgeBasedSemanticRoleLabeler group configuration";
    throw InvalidConfiguration();
  }

  try
  {
    mode = QString::fromUtf8(unitConfiguration.getParamsValueAtKey("mode").c_str());
    if (mode != "VerbNet" && mode != "FrameNet")
    {
      SEMANTICANALYSISLOGINIT;
      LERROR << "Unknown semantic annotation mode" << mode;
      throw InvalidConfiguration();
    }
  }
  catch (NoSuchParam& )
  {
    // keep default
  }
  
  // Initialize the python SRL system
  /*
   * Find the first python executable in the path and use it as the program name.
   * 
   * This allows to find the modules set up in an activated virtualenv
   */
  QString str_program_name;
  QString pathEnv = QString::fromUtf8(qgetenv("PATH").constData());
  for (const auto & path: pathEnv.split(QRegExp("[;:]")))
  {
    if (QFile::exists(path + "/python" ))
    {
      str_program_name = path + "/python";
      break;
    }
  }
  Py_SetProgramName(const_cast<wchar_t*>( str_program_name.toStdWString().c_str()));

  
  Py_Initialize();
  
  PyObject* main_module = PyImport_ImportModule("__main__");
  PyObject* main_dict = PyModule_GetDict(main_module);
  PyObject* sys_module = PyImport_ImportModule("sys");
  if (sys_module == NULL)
  {
    LERROR << "Failed to import the sys module";
    PyErr_Print();
  }
  PyObject* sys_dict = PyModule_GetDict(sys_module);
  PyDict_SetItemString(main_dict, "sys", sys_module);  

  // Add the path to the knowledgesrl pachkage to putho path
  PyObject* pythonpath = PySys_GetObject("path");
  if (PyList_Append(pythonpath, PyUnicode_DecodeFSDefault("/home/gael/Projets/knowledgesrl/src")) ==  -1)
  {
    LERROR << "Failed to append to python path";
    PyErr_Print();
    Py_Exit(1);
  }
  
  // Import the semanticrolelabeller module
  PyObject* semanticrolelabeller_module = PyImport_ImportModule("semanticrolelabeller");
  if (semanticrolelabeller_module == NULL)
  {
    LERROR << "Failed to import srl semanticrolelabeller module";
    PyErr_Print();
    Py_Exit(1);
  }
  
  // Create the semantic role labeller instance
  m_d->m_instance = PyObject_CallMethod(semanticrolelabeller_module, "SemanticRoleLabeller", "[s]", QString("--log=%1").arg(kbsrlLogLevel).toUtf8().constData());
  if (m_d->m_instance == NULL)
  {
    LERROR << "Cannot instantiate the SemanticRoleLabeller python class";
    PyErr_Print();
    Py_Exit(1);
  }
}

LimaStatusCode KnowledgeBasedSemanticRoleLabeler::process(
  AnalysisContent& analysis) const
{
  TimeUtilsController knowledgeBasedSemanticRoleLabelerProcessTime("KnowledgeBasedSemanticRoleLabeler");
  SEMANTICANALYSISLOGINIT;
  LINFO << "start SRL process";
  
  LinguisticMetaData* metadata=static_cast<LinguisticMetaData*>(analysis.getData("LinguisticMetaData"));
  if (metadata == 0) {
      LERROR << "no LinguisticMetaData ! abort";
      return MISSING_DATA;
  }

  // Use CoNLL duper to produce the input to the SRL
  LimaStatusCode returnCode(SUCCESS_ID);
  returnCode=m_d->m_dumper->process(analysis);
  if (returnCode!=SUCCESS_ID) {
    LERROR << "KnowledgeBasedSemanticRoleLabeler: failed to dump data to temporary file";
    return returnCode;
  }

  QString fileName = QString::fromUtf8(metadata->getMetaData("FileName").c_str());
  QString inputFilename, outputFilename;
  if (!m_d->m_inputSuffix.isEmpty())
  {
    inputFilename = fileName+ m_d->m_inputSuffix;
  }
  QFile inputFile(inputFilename);
  inputFile.open(QIODevice::ReadOnly);
  QString conllInput = QString::fromUtf8(inputFile.readAll().constData());
  inputFile.close();
  if (!m_d->m_outputSuffix.isEmpty())
  {
    outputFilename = fileName + m_d->m_outputSuffix;
  }

  // Run the semantic role labeller
  PyObject* callResult = PyObject_CallMethod(m_d->m_instance, "annotate", "s", conllInput.toUtf8().constData());
  if (callResult == NULL)
  {
    LERROR << "Failed to call the annotate method";
    PyErr_Print();
    Py_Exit(1);
  }
  
  // Display the SRL result
  char* result = PyUnicode_AsUTF8(callResult);
  if (result == NULL)
  {
    LERROR << "Cannot convert result item to string";
    PyErr_Print();
    Py_Exit(1);
  }
  LDEBUG << "Python result is:" << result;
  QFile outputFile(outputFilename);
  outputFile.open(QIODevice::WriteOnly);
  outputFile.write(result);
  outputFile.close();
  
  // Import the CoNLL result
  returnCode=m_d->m_loader->process(analysis);
  if (returnCode!=SUCCESS_ID) {
    LERROR << "KnowledgeBasedSemanticRoleLabeler: failed to load data from temporary file";
    return returnCode;
  }

  
  return returnCode;
}

} //namespace SemanticAnalysis
} // namespace LinguisticProcessing
} // namespace Lima
