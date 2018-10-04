// Copyright 2017, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Authors: Patrick Brosi <brosi@informatik.uni-freiburg.de>

#ifndef OCTI_TRANSITGRAPH_TRANSITGRAPH_H_
#define OCTI_TRANSITGRAPH_TRANSITGRAPH_H_

#include "octi/transitgraph/TransitEdgePL.h"
#include "octi/transitgraph/TransitNodePL.h"
#include "util/geo/Geo.h"
#include "util/geo/Grid.h"
#include "util/graph/UndirGraph.h"

using util::geo::Grid;
using util::geo::Point;

namespace octi {
namespace transitgraph {

typedef util::graph::Node<TransitNodePL, TransitEdgePL> TransitNode;
typedef util::graph::Edge<TransitNodePL, TransitEdgePL> TransitEdge;

typedef Grid<TransitNode*, Point, double> NodeGrid;
typedef Grid<TransitEdge*, Line, double> EdgeGrid;

struct ISect {
  TransitEdge *a, *b;

  util::geo::LinePoint<double> bp;
};

class TransitGraph
    : public util::graph::UndirGraph<TransitNodePL, TransitEdgePL> {
 public:
  TransitGraph();

  void readFromJson(std::istream* s);
  void readFromDot(std::istream* s);

  const util::geo::Box<double>& getBBox() const;
  void topologizeIsects();

 private:
  util::geo::Box<double> _bbox;

  ISect getNextIntersection();

  void buildGrids();

  void addRoute(const Route* r);
  const Route* getRoute(const std::string& id) const;

  void expandBBox(const Point<double>& p);

  std::set<TransitEdge*> proced;
  std::map<std::string, const Route*> _routes;

  NodeGrid _nodeGrid;
  EdgeGrid _edgeGrid;
};

}  // transitgraph
}  // octi

#endif  // OCTI_TRANSITGRAPH_TRANSITGRAPH_H_