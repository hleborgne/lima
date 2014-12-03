/*
    Copyright 2002-2014 CEA LIST

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
/************************************************************************
 *
 * @file       SemanticRoleLabelingLoader.cpp
 * @author     Clémence Filmont <clemence.filmont@cea.fr>
 * @date       2014
 * copyright   Copyright (C) 2014 by CEA LIST
 ***********************************************************************/

#include "SemanticRoleLabelingLoader.h"
#include "LimaConllTokenIdMapping.h"

#include "common/AbstractFactoryPattern/SimpleFactory.h"
#include "common/Data/strwstrtools.h"
#include "common/MediaticData/mediaticData.h"
#include "linguisticProcessing/LinguisticProcessingCommon.h"
#include "linguisticProcessing/common/annotationGraph/AnnotationData.h"
#include "linguisticProcessing/common/annotationGraph/AnnotationGraph.h"
#include "linguisticProcessing/core/Automaton/recognizerData.h"
#include "linguisticProcessing/core/LinguisticAnalysisStructure/LinguisticGraph.h"
#include "linguisticProcessing/core/LinguisticProcessors/LinguisticMetaData.h"
#include "linguisticProcessing/core/LinguisticAnalysisStructure/AnalysisGraph.h"

#include <QRegExp>
#include "QStringList"
#include <QString>
#include <QFile>


#include <utility>
#include <iostream>
#include <fstream>
#include <map>

using namespace std;
using namespace Lima::LinguisticProcessing::LinguisticAnalysisStructure;
using namespace Lima::LinguisticProcessing::ApplyRecognizer;
using namespace Lima::Common::XMLConfigurationFiles;
using namespace Lima::Common::AnnotationGraphs;
using namespace Lima::LinguisticProcessing::SemanticAnalysis;
using namespace Lima::Common::AnnotationGraphs;


namespace Lima {
namespace LinguisticProcessing {
namespace SemanticAnalysis {

SimpleFactory<MediaProcessUnit,SemanticRoleLabelingLoader> SemanticRoleLabelingFactory(SEMANTICROLELABELINGLOADER_CLASSID);

// Conll handler
struct ConllHandler
{
  ConllHandler(MediaId language, AnalysisContent& analysis, LinguisticAnalysisStructure::AnalysisGraph* graph);
  virtual ~ConllHandler();

  /**
    * @brief extract semantic annotations associated to token
    * @param sentenceIndex the index of the current sentence
    * @param limaConllMapping the chosen lima conll token id mapping
    * @param sentence the current sentence
    * @return true if any verbal class is found, false otherwise
    */
  bool extractSemanticInformation(int sentenceIndex, LimaConllTokenIdMapping* limaConllMapping, const QString & sentence);

  /**
    * @brief split a text into different types segments
    * @param segment the segment to split
    * @param separator the separator used to split
    * @return the segment split
    */
  QStringList splitSegment(const QString & segment, QRegExp separator);

  /**
    * @brief get the lima token id matching any conll token one from the same text
    * @param conllTokenId the conll token id one search the matched lima id
    * @param sentenceNb the index of the current sentence
    * @param limaConllMapping the chosen lima conll token id mapping
    * @return the lima token id
    * @note function to put in the LimaConllTokenIdMapping class?
    */
  LinguisticGraphVertex getLimaTokenId(int conllTokenId, int sentenceIndex, LimaConllTokenIdMapping* limaConllMapping);


  MediaId m_language;
  AnalysisContent& m_analysis;
  LinguisticAnalysisStructure::AnalysisGraph* m_graph;
  QRegExp m_descriptorSeparator;
  QRegExp m_tokenSeparator;
  QVector< QPair<LinguisticGraphVertex,QString> > m_verbalClasses;
  QVector < std::vector<std::pair<LinguisticGraphVertex,QString>> >m_semanticRoles;
  int m_verbalClassNb;
};


class SemanticRoleLabelingLoaderPrivate
{
  friend class SemanticRoleLabelingLoader;
  SemanticRoleLabelingLoaderPrivate();
  ~SemanticRoleLabelingLoaderPrivate();

  MediaId m_language;
  std::string m_graph;
};



//***********************************************************************
SemanticRoleLabelingLoaderPrivate::SemanticRoleLabelingLoaderPrivate():
m_language(0),
m_graph("PosGraph")
{}

SemanticRoleLabelingLoaderPrivate::~SemanticRoleLabelingLoaderPrivate()
{
}

//***********************************************************************
SemanticRoleLabelingLoader::SemanticRoleLabelingLoader():
    AnalysisLoader(),
    m_d(new SemanticRoleLabelingLoaderPrivate())
{
}

SemanticRoleLabelingLoader::~SemanticRoleLabelingLoader()
{
  delete m_d;
}

//***********************************************************************

void SemanticRoleLabelingLoader::init(Common::XMLConfigurationFiles::GroupConfigurationStructure& unitConfiguration, Manager* manager)
{

  SEMANTICANALYSISLOGINIT;
  m_d->m_language=manager->getInitializationParameters().media;
  AnalysisLoader::init(unitConfiguration,manager);
  try
  {
    m_d->m_graph=unitConfiguration.getParamsValueAtKey("graph");
  }
  catch (NoSuchParam& ) {} // keep default value
}


LimaStatusCode SemanticRoleLabelingLoader::process(AnalysisContent& analysis) const
{
  SEMANTICANALYSISLOGINIT;
  AnalysisGraph* tokenList=static_cast<AnalysisGraph*>(analysis.getData(m_d->m_graph));
  if (tokenList==0)
  {
    LERROR << "graph " << m_d->m_graph << " has not been produced: check pipeline" ;
    return MISSING_DATA;
  }
  AnnotationData* annotationData = static_cast<AnnotationData*>(analysis.getData("AnnotationData"));
  LimaConllTokenIdMapping* limaConllMapping = static_cast<LimaConllTokenIdMapping*>(analysis.getData("LimaConllTokenIdMapping"));

  LinguisticMetaData* metadata=static_cast<LinguisticMetaData*>(analysis.getData("LinguisticMetaData"));
  if (metadata == 0)
  {
      LERROR << "no LinguisticMetaData ! abort";
      return MISSING_DATA;
  }
  
  QString fileName = QString::fromUtf8((metadata->getMetaData("FileName")+m_inputFileExtension).c_str());
  QFile file(fileName);


  if (!file.open(QIODevice::ReadOnly))
  {
    LERROR << "cannot open file" << fileName;
    return CANNOT_OPEN_FILE_ERROR;
  }
  int sentenceNb=1;
  std::map <int, QString> sentences;
  while (!file.atEnd())
  {
    QByteArray text=file.readLine();
    QString textString = QString::fromUtf8(text.constData());
    //One assume that the input file does not start with a blank line
    if (textString.size()<3)
    {
      sentenceNb++;
    }
    else
    {
      QString becomingSentence=sentences[sentenceNb]+textString;
      sentences[sentenceNb]= becomingSentence;
    }
  }

  ConllHandler cHandler(m_d->m_language, analysis, tokenList);
  for (std::map<int,QString>::iterator it=sentences.begin(); it!=sentences.end(); ++it)
  {
    int sentenceIndex=it->first;
    QString sentence=it->second;
    if(cHandler.extractSemanticInformation(sentenceIndex, limaConllMapping,sentence)){
      LDEBUG << "SemanticRoleLabelingLoader::process there is/are " << cHandler.m_verbalClassNb << "verbal class(es) for this sentence " ;
      for (int vClassIndex=0;vClassIndex<cHandler.m_verbalClassNb;vClassIndex++){
        LinguisticGraphVertex posGraphPredicateVertex=cHandler.m_verbalClasses[vClassIndex].first;
        LimaString verbalClass=cHandler.m_verbalClasses[vClassIndex].second;

        AnnotationGraphVertex annotPredicateVertex=annotationData->createAnnotationVertex();
        annotationData->addMatching("PosGraph", posGraphPredicateVertex, "annot", annotPredicateVertex);
        annotationData->annotate(annotPredicateVertex, "Predicate", verbalClass);


        LDEBUG << "SemanticRoleLabelingLoader::process: annotation vertex"<< annotPredicateVertex <<"was created for the verbal class "<< annotationData->stringAnnotation(annotPredicateVertex, "Predicate") << "and the PoS graph vertex"<<posGraphPredicateVertex;
        std::vector <pair<LinguisticGraphVertex,QString>>::iterator semRoleIt;
        for (semRoleIt=cHandler.m_semanticRoles[vClassIndex].begin();         semRoleIt!=cHandler.m_semanticRoles[vClassIndex].end();semRoleIt++){
          LinguisticGraphVertex posGraphRoleVertex=(*semRoleIt).first;
          LimaString semanticRole=(*semRoleIt).second;

          AnnotationGraphVertex annotRoleVertex=annotationData->createAnnotationVertex();
          AnnotationGraphEdge roleEdge=annotationData->createAnnotationEdge(annotPredicateVertex, annotRoleVertex);
          annotationData->annotate(roleEdge, "SemanticRole", semanticRole);
          annotationData->addMatching("PosGraph", posGraphRoleVertex, "annot", annotRoleVertex);


          LDEBUG << "SemanticRoleLabelingLoader::process: annotation edge" << roleEdge << "annotated " << annotationData->stringAnnotation(roleEdge, "SemanticRole")<< "was created for" << verbalClass << " and the PoS graph vertices " << posGraphPredicateVertex << "and" << posGraphRoleVertex ;
        }
      }
    }
  }
  return SUCCESS_ID;
}



ConllHandler::ConllHandler(MediaId language, AnalysisContent& analysis, LinguisticAnalysisStructure::AnalysisGraph* graph):
m_language(language),
m_analysis(analysis),
m_graph(graph),
m_descriptorSeparator("\t+"),
m_tokenSeparator("\n+"),
m_verbalClasses(),
m_semanticRoles(),
m_verbalClassNb()
{
}
ConllHandler::~ConllHandler()
{
}

// designed to be repeated on each sentence
bool ConllHandler::extractSemanticInformation(int sentenceI, LimaConllTokenIdMapping* limaConllMapping, const QString & sent)
{
  SEMANTICANALYSISLOGINIT;
  ConllHandler cHandler(m_language, m_analysis, m_graph);
  QStringList sentenceTokens=cHandler.splitSegment(sent, m_tokenSeparator);
  QStringList::const_iterator tokensIterator;
  QString firstSentenceToken=(*sentenceTokens.constBegin());
  int descriptorsNb=cHandler.splitSegment(firstSentenceToken, m_descriptorSeparator).size();
  m_verbalClassNb=descriptorsNb-11;
  int classIndex=0;
  if (m_verbalClassNb!=0)
  {
    LDEBUG << sentenceI << " : \n" << sent ;
    m_verbalClasses.clear();
    m_verbalClasses.resize(m_verbalClassNb);
    m_semanticRoles.clear();
    m_semanticRoles.resize(m_verbalClassNb);
    //repeated on each token of the sentence, that is on each line
    for (tokensIterator = sentenceTokens.constBegin(); tokensIterator != sentenceTokens.constEnd();
            ++tokensIterator)
    {
      int  roleNumbers=0;
      QStringList descriptors=cHandler.splitSegment((*tokensIterator),m_descriptorSeparator);
      if (descriptors.size()>=10)
      {
        int conllTokenId=descriptors[0].toInt();
        QString conllToken=descriptors[1];
        if(descriptors[10]!="-")
        {
            QString verbalClass=descriptors[10];
            QString vClass=descriptors[10];
            LinguisticGraphVertex limaTokenId=cHandler.getLimaTokenId(conllTokenId, sentenceI, limaConllMapping);
            m_verbalClasses[classIndex]=qMakePair(limaTokenId, vClass);
            classIndex++;
        }
        for (int roleTargetFieldIndex=0; roleTargetFieldIndex<m_verbalClassNb;roleTargetFieldIndex++)
        {
          if (descriptors[11+roleTargetFieldIndex]!="-")
          {
            QString semanticRoleLabel=descriptors[11+roleTargetFieldIndex];

            LinguisticGraphVertex limaTokenId=cHandler.getLimaTokenId(conllTokenId, sentenceI,   limaConllMapping);
            if(limaTokenId!=0)
            {
              LDEBUG << "ConllHandler::extractSemanticInformation The PoS graph token id matching the conll token id " << conllTokenId << " is " << limaTokenId;
              std::vector<std::pair<LinguisticGraphVertex,QString>> sRoles;
              m_semanticRoles[roleTargetFieldIndex].push_back(make_pair(limaTokenId,semanticRoleLabel));
            }
            roleNumbers++;
          }
        }
      }
    }
  }
  return classIndex;
}



QStringList ConllHandler::splitSegment(const QString & segment, QRegExp separator)
{
  QStringList segmentsSplited;
  segmentsSplited =segment.split(QRegExp(separator),QString::SkipEmptyParts);
  return segmentsSplited;
}

LinguisticGraphVertex ConllHandler::getLimaTokenId(int conllTokenId, int sentenceI, LimaConllTokenIdMapping* limaConllMapping)
{
  SEMANTICANALYSISLOGINIT;
  std::map< int,std::map< int,LinguisticGraphVertex>>::iterator limaConllMappingIt;
  limaConllMappingIt=limaConllMapping->find(sentenceI);
  if (limaConllMappingIt == limaConllMapping->end())
  {
    LERROR << "Sentence " << sentenceI << " not found";
    return 0;
  }
  std::map< int,LinguisticGraphVertex> limaConllId=(*limaConllMappingIt).second;
  std::map< int,LinguisticGraphVertex>::iterator limaConllIdIt=limaConllId.find(conllTokenId);
  if (limaConllIdIt==limaConllId.end())
  {
    LERROR << "Conll token id " << conllTokenId << " not found";
    return 0;
  }
  LinguisticGraphVertex limaTokenId=limaConllIdIt->second;
  return limaTokenId;
}

}
}
} // end namespace
