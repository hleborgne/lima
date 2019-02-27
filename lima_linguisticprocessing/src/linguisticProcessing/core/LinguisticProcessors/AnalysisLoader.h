/*
    Copyright 2002-2013 CEA LIST

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
 * @author     besancon (besanconr@zoe.cea.fr)
 * @date       Mon Jan 17 2011
 * @brief abstract class for analysis loaders
 ***********************************************************************/
#ifndef LIMA_LINGUISTICPROCESSING_ANALYSISLOADER_H
#define LIMA_LINGUISTICPROCESSING_ANALYSISLOADER_H

#include "LinguisticProcessorsExport.h"
#include "common/MediaProcessors/MediaProcessUnit.h"
#include <QSharedPointer>
#include <iostream>
#include <fstream>

namespace Lima {
namespace LinguisticProcessing {

#define ANALYSISLOADER_CLASSID "AnalysisLoader"

class AnalysisLoaderPrivate;
/*
 * @brief this is the abstract class for analysis loaders, that read
 * informations from external files to insert them in the analysis
 * data
 */
class LIMA_LINGUISTICPROCESSORS_EXPORT AnalysisLoader : public MediaProcessUnit
{
  friend class AnalysisLoaderPrivate;
public:
  AnalysisLoader();

  virtual ~AnalysisLoader();

  void init(Common::XMLConfigurationFiles::GroupConfigurationStructure& unitConfiguration,
            Manager* manager) override;

  LimaStatusCode process(AnalysisContent& /*analysis*/) const override { return SUCCESS_ID; }

  QString getInputFile(AnalysisContent& analysis) const;

protected:
  QSharedPointer<AnalysisLoaderPrivate> m_d;
};

} // end namespace
} // end namespace

#endif
