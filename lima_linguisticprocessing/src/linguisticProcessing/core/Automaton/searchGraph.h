/*
    Copyright 2002-2018 CEA LIST

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
 * @file       searchGraph.h
 * @author     besancon (besanconr@zoe.cea.fr)
 * @date       Tue May 24 2005
 * @version    $Id$
 * copyright   Copyright (C) 2005-2018 by CEA LIST
 * Project     Automaton
 *
 * @brief classes to abstract the sense of search in the graph
 * (forward/backward)
 *
 ***********************************************************************/
#ifndef SEARCHGRAPH_H
#define SEARCHGRAPH_H

#include "AutomatonExport.h"
#include "linguisticProcessing/core/LinguisticAnalysisStructure/AnalysisGraph.h"
#include <deque>

namespace Lima {
namespace LinguisticProcessing {
namespace Automaton {

/**
 * enumerated type to indicate in which sense the automaton should
 * be built or searched
 *
 */
enum SearchGraphSense {
    FORWARDSEARCH, /**< forward search in the graph */
    BACKWARDSEARCH  /**< backward search in the graph */
};

class LIMA_AUTOMATON_EXPORT SearchGraph {
public:
    SearchGraph() {}
    virtual ~SearchGraph() {}
    virtual void findNextVertices(const LinguisticGraph* graph,
                                  const LinguisticGraphVertex& current) = 0;
    virtual bool getNextVertex(const LinguisticGraph* graph,
                               LinguisticGraphVertex& next) = 0;
    virtual SearchGraph* createNew() const = 0;
    virtual LinguisticGraphVertex endOfGraph(const LinguisticAnalysisStructure::AnalysisGraph& graph) = 0;
    virtual void clear() = 0;
    virtual void reinit() = 0;
};

#ifdef DEBUG_LP
LIMA_AUTOMATON_EXPORT std::ostream& output(std::ostream& os, const SearchGraph *x, const LinguisticGraph* graph);
#endif

class LIMA_AUTOMATON_EXPORT BackwardSearch :
            public SearchGraph
{
public:
    BackwardSearch();
    ~BackwardSearch() {}
    void findNextVertices(const LinguisticGraph* graph,
                          const LinguisticGraphVertex& current) override;
    bool getNextVertex(const LinguisticGraph* graph,
                       LinguisticGraphVertex& next) override;
    SearchGraph* createNew() const override;
    LinguisticGraphVertex endOfGraph(const LinguisticAnalysisStructure::AnalysisGraph& graph) override;
    void clear()  override{
        m_current.pop_back();
    }
    void reinit()  override{
        m_current.clear();
    }

    typedef std::pair<LinguisticGraphVertex,std::pair<LinguisticGraphInEdgeIt,LinguisticGraphInEdgeIt> > Vertex2EdgePair;

private:
    // use a deque for a stack so that can be clear()ed
    std::deque<Vertex2EdgePair> m_current;

#ifdef DEBUG_LP
    friend LIMA_AUTOMATON_EXPORT std::ostream& output(std::ostream& os, const BackwardSearch::Vertex2EdgePair& x, const LinguisticGraph* graph);
    friend LIMA_AUTOMATON_EXPORT std::ostream& output(std::ostream& os, const BackwardSearch& x, const LinguisticGraph* graph);
#endif
};

class LIMA_AUTOMATON_EXPORT ForwardSearch :
            public SearchGraph
{
public:
    ForwardSearch();
    ~ForwardSearch() {}
    void findNextVertices(const LinguisticGraph* graph,
                          const LinguisticGraphVertex& current) override;
    bool getNextVertex(const LinguisticGraph* graph,
                       LinguisticGraphVertex& next) override;
    SearchGraph* createNew() const override;
    LinguisticGraphVertex endOfGraph(const LinguisticAnalysisStructure::AnalysisGraph& graph) override;
    void clear() override {
        m_current.pop_back();
    }
    void reinit() override {
        m_current.clear();
    }

    typedef std::pair<LinguisticGraphVertex,std::pair<LinguisticGraphOutEdgeIt,LinguisticGraphOutEdgeIt> > Vertex2EdgePair;

private:
    // use a deque for a stack so that can be clear()ed
    std::deque<Vertex2EdgePair> m_current;

#ifdef DEBUG_LP
    friend LIMA_AUTOMATON_EXPORT std::ostream& output(std::ostream& os, const ForwardSearch::Vertex2EdgePair& x, const LinguisticGraph* graph);
    friend LIMA_AUTOMATON_EXPORT std::ostream& output(std::ostream& os, const ForwardSearch& x, const LinguisticGraph* graph);
#endif
};

} // end namespace
} // end namespace
} // end namespace

#endif
