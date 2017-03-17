// Copyright 2016, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Authors: Patrick Brosi <brosi@informatik.uni-freiburg.de>

#include <cassert>
#include "./Node.h"
#include "./Edge.h"
#include "./Route.h"
#include "./TransitGraph.h"
#include "./../util/Geo.h"
#include "./../geo/BezierCurve.h"
#include "../graph/OrderingConfiguration.h"

using namespace transitmapper;
using namespace graph;
using namespace gtfsparser;

using util::geo::Point;
using util::geo::Line;

// _____________________________________________________________________________
Point NodeFront::getTripOccPosUnder(const Route* r,
    const graph::Configuration& c, const Edge* e, const Ordering* order) const {

  RouteOccWithPos to;

  if (edge == e) {
    to = edge->getTripsForRouteUnder(r, *order);
  } else {
    to = edge->getTripsForRouteUnder(r, c.find(edge)->second);
  }

  if (to.first) {
    return getTripPos(edge, to.second, n == edge->getTo());
  }
}

// _____________________________________________________________________________
Point NodeFront::getTripPos(const Edge* e, size_t pos,
    bool inv) const {
  double p;
  if (!inv) {
    p = (e->getWidth() + e->getSpacing()) * pos
        + e->getWidth()/2;
  } else {
    p = (e->getWidth() + e->getSpacing()) * (e->getCardinality() - 1 - pos)
        + e->getWidth()/2;
  }

  // use interpolate here directly for speed
  return geom.interpolate(geom.getLine().front(), geom.getLine().back(), p);
}

// _____________________________________________________________________________
double Node::getMaxNodeFrontWidth() const {
  double ret = 0;
  for (const NodeFront& g : _mainDirs) {
    if (g.edge->getTotalWidth() > ret) ret = g.edge->getTotalWidth();
  }
  return ret;
}

// _____________________________________________________________________________
Node::Node(const std::string& id, Point pos) : _id(id), _pos(pos) {
}

// _____________________________________________________________________________
Node::Node(const std::string& id, double x, double y) : _id(id), _pos(x, y) {
}

// _____________________________________________________________________________
Node::Node(const std::string& id, Point pos, StationInfo s) : _id(id), _pos(pos) {
  addStop(s);
}

// _____________________________________________________________________________
Node::Node(const std::string& id, double x, double y, StationInfo s) : _id(id), _pos(x, y) {
  addStop(s);
}

// _____________________________________________________________________________
Node::~Node() {
  for (auto e = _adjListOut.begin(); e != _adjListOut.end();) {
    Edge* eP = *e;

    if (eP->getFrom() == this) {
      // careful with invalidating iterators
      e = _adjListOut.erase(e);
    } else {
      eP->getFrom()->removeEdge(eP);
      e++;
    }

    eP->getTo()->removeEdge(eP);

    delete eP;
  }

  for (auto e = _adjListIn.begin(); e != _adjListIn.end();) {
    Edge* eP = *e;

    if (eP->getTo() == this) {
      // careful with invalidating iterators
      e = _adjListIn.erase(e);
    } else {
      eP->getTo()->removeEdge(eP);
      e++;
    }

    eP->getFrom()->removeEdge(eP);

    delete eP;
  }
}

// _____________________________________________________________________________
void Node::addStop(StationInfo s) {
  _stops.push_back(s);
}

// _____________________________________________________________________________
const std::vector<StationInfo>& Node::getStops() const {
  return _stops;
}

// _____________________________________________________________________________
void Node::addEdge(Edge* e) {
  if (e->getFrom() == this) _adjListOut.insert(e);
  if (e->getTo() == this) _adjListIn.insert(e);
}

// _____________________________________________________________________________
void Node::removeEdge(Edge* e) {
  if (e->getFrom() == this) _adjListOut.erase(e);
  if (e->getTo() == this) _adjListIn.erase(e);

  for (size_t i = 0; i < _mainDirs.size(); i++) {
    if (_mainDirs[i].edge == e) {
      _mainDirs.erase(_mainDirs.begin() + i);
    }
  }

  // TODO: remove from _routeConnExceptions
}

// _____________________________________________________________________________
const Point& Node::getPos() const {
  return _pos;
}

// _____________________________________________________________________________
void Node::setPos(const Point& p) {
  _pos = p;
}

// _____________________________________________________________________________
const std::string& Node::getId() const {
  return _id;
}

// _____________________________________________________________________________
void Node::addMainDir(NodeFront f) {
  _mainDirs.push_back(f);
}

// _____________________________________________________________________________
const NodeFront* Node::getNodeFrontFor(const Edge* e) const {
  for (auto& nf : getMainDirs()) {
    if (nf.edge == e) {
      return &nf;
    }
  }

  return 0;
}

// _____________________________________________________________________________
double Node::getScore(const Configuration& c) const {
  std::vector<InnerGeometry> igs = getInnerGeometries(c, -1);

  double score = 0;

  for (size_t i = 0; i < igs.size(); i++) {
    for (size_t j = 0; j < igs.size(); j++) {
      if (j == i) continue;  // don't check against itself

      if (igs[j].geom.distTo(igs[i].geom) < 1) {
        score += .5;
      }
    }
  }

  return score / sqrt(_adjListIn.size() + _adjListOut.size());
}

// _____________________________________________________________________________
double Node::getScoreUnder(const graph::Configuration& c, const Edge* e,
    const std::vector<size_t>* order) const {

  std::vector<InnerGeometry> igs = getInnerGeometriesUnder(c, -1, e, order);

  double score = 0;

  for (size_t i = 0; i < igs.size(); i++) {
    for (size_t j = 0; j < igs.size(); j++) {
      if (j == i) continue;  // don't check against itself

      if (igs[j].geom.distTo(igs[i].geom) < 1) {
        score += .5;
      }
    }
  }

  return score / sqrt(_adjListIn.size() + _adjListOut.size());
}

// _____________________________________________________________________________
double Node::getAreaScore(const Configuration& c, const Edge* e,
  const Ordering* order)
const {
  double ret = getScoreUnder(c, e, order);

  for (auto e : _adjListIn) {
    ret += e->getFrom()->getScoreUnder(c, e, order);
  }

  for (auto e : _adjListOut) {
    ret += e->getTo()->getScoreUnder(c, e, order);
  }

  return ret;
}

// _____________________________________________________________________________
double Node::getAreaScore(const Configuration& c) const {
  return getAreaScore(c, 0, 0);
}

// _____________________________________________________________________________
std::vector<Partner> Node::getPartners(const NodeFront* f,
  const RouteOccurance& ro) const {
  std::vector<Partner> ret;
  for (const auto& nf : getMainDirs()) {
    if (&nf == f) continue;

    for (const RouteOccurance& to : nf.edge->getContinuedRoutesIn(this, ro.route, ro.direction, f->edge)) {
      Partner p;
      p.front = &nf;
      p.edge = nf.edge;
      p.route = to.route;
      ret.push_back(p);
    }
  }
  return ret;
}

// _____________________________________________________________________________
std::vector<InnerGeometry> Node::getInnerGeometries(const Configuration& c,
    double prec) const {
  return getInnerGeometriesUnder(c, prec, 0, 0);
}

// _____________________________________________________________________________
geo::PolyLine Node::getInnerStraightLine(const Configuration& c,
    const NodeFront& nf, const RouteOccurance& tripOcc,
    const graph::Partner& partner, const Edge* e,
    const graph::Ordering* order) const {
  Point p = nf.getTripOccPosUnder(tripOcc.route, c, e, order);
  Point pp = partner.front->getTripOccPosUnder(partner.route, c, e, order);

  return geo::PolyLine(p, pp);
}

// _____________________________________________________________________________
geo::PolyLine Node::getInnerBezier(const Configuration& cf, const NodeFront& nf,
    const RouteOccurance& tripOcc, const graph::Partner& partner,
    const Edge* e,
    const graph::Ordering* order,
    double prec) const {


  Point p = nf.getTripOccPosUnder(tripOcc.route, cf, e, order);
  Point pp = partner.front->getTripOccPosUnder(partner.route, cf, e, order);

  double d = util::geo::dist(p, pp) / 2;

  Point b = p;
  Point c = pp;
  std::pair<double, double> slopeA, slopeB;

  if (nf.edge->getTo() == this) {
    slopeA = nf.edge->getGeom().getSlopeBetweenDists(nf.edge->getGeom().getLength() - 5, nf.edge->getGeom().getLength());
  } else {
    slopeA = nf.edge->getGeom().getSlopeBetweenDists(5, 0);
  }

  if (partner.front->edge->getTo() == this) {
    slopeB = partner.front->edge->getGeom().getSlopeBetweenDists(partner.front->edge->getGeom().getLength() - 5, partner.front->edge->getGeom().getLength());
  } else {
    slopeB = partner.front->edge->getGeom().getSlopeBetweenDists(5, 0);
  }

  b = Point(p.get<0>() + slopeA.first * d, p.get<1>() + slopeA.second * d);
  c = Point(pp.get<0>() + slopeB.first * d, pp.get<1>() + slopeB.second * d);

  geo::BezierCurve bc(p, b, c, pp);
  return bc.render(prec);
}

// _____________________________________________________________________________
std::vector<InnerGeometry> Node::getInnerGeometriesUnder(
    const graph::Configuration& c, double prec,
    const Edge* e, const graph::Ordering* order) const {
  std::vector<InnerGeometry> ret;

  std::map<const Route*, std::set<const NodeFront*> > processed;

  for (size_t i = 0; i < getMainDirs().size(); ++i) {
    const graph::NodeFront& nf = getMainDirs()[i];

    const std::vector<size_t>* ordering = &c.find(nf.edge)->second;

    if (nf.edge == e) {
      ordering = order;
    }

    for (size_t j : *ordering) {
      const RouteOccurance& routeOcc = (*nf.edge->getTripsUnordered())[j];

      std::vector<graph::Partner> partners = getPartners(&nf, routeOcc);

      for (const graph::Partner& p : partners) {
        if (processed[routeOcc.route].find(p.front) != processed[routeOcc.route].end()) {
          continue;
        }

        if (prec > 0) {
          ret.push_back(InnerGeometry(
              getInnerBezier(c, nf, routeOcc, p, e, order, prec),
              p.route, nf.edge));
        } else {
          ret.push_back(InnerGeometry(
              getInnerStraightLine(c, nf, routeOcc, p, e, order),
              p.route, nf.edge));
        }
      }

      processed[routeOcc.route].insert(&nf);
    }


  }

  return ret;
}

// _____________________________________________________________________________
util::geo::Polygon Node::getConvexFrontHull(double d) const {
  util::geo::MultiLine l;

  if (getMainDirs().size() != 2) {
    for (auto& nf : getMainDirs()) {

      geo::PolyLine capped = nf.geom.getSegment(
          (nf.edge->getWidth() / 2) / nf.geom.getLength(),
          (nf.geom.getLength() - nf.edge->getWidth() / 2) / nf.geom.getLength());

      l.push_back(capped.getLine());
    }
  } else {
    // for two main dirs, take average
    std::vector<const geo::PolyLine*> pols;
    geo::PolyLine a = getMainDirs()[0].geom.getSegment(
          (getMainDirs()[0].edge->getWidth() / 2) / getMainDirs()[0].geom.getLength(),
          (getMainDirs()[0].geom.getLength() - getMainDirs()[0].edge->getWidth() / 2) / getMainDirs()[0].geom.getLength());
    geo::PolyLine b = getMainDirs()[1].geom.getSegment(
          (getMainDirs()[1].edge->getWidth() / 2) / getMainDirs()[1].geom.getLength(),
          (getMainDirs()[1].geom.getLength() - getMainDirs()[1].edge->getWidth() / 2) / getMainDirs()[1].geom.getLength());

    assert(a.getLine().size() > 1);
    assert(b.getLine().size() > 1);

    if (util::geo::dist(a.getLine()[0], b.getLine()[0]) > util::geo::dist(a.getLine()[1], b.getLine()[0])) {
      a.reverse();
    }

    pols.push_back(&a);
    pols.push_back(&b);
    l.push_back(geo::PolyLine::average(pols).getLine());
  }

  util::geo::MultiPolygon ret;
  double pointsPerCircle = 36;
  bgeo::strategy::buffer::distance_symmetric<double> distanceStrat(d);
  bgeo::strategy::buffer::join_round joinStrat(pointsPerCircle);
  bgeo::strategy::buffer::end_round endStrat(pointsPerCircle);
  bgeo::strategy::buffer::point_circle circleStrat(pointsPerCircle);
  bgeo::strategy::buffer::side_straight sideStrat;

  if (l.size() > 1) {
    util::geo::Polygon hull;
    bgeo::convex_hull(l, hull);
    bgeo::buffer(hull, ret, distanceStrat, sideStrat, joinStrat, endStrat, circleStrat);
  } else {
    bgeo::buffer(l, ret, distanceStrat, sideStrat, joinStrat, endStrat, circleStrat);
  }

  assert(ret.size() > 0);
  return ret[0];
}

// _____________________________________________________________________________
size_t Node::getNodeFrontPos(const NodeFront* a) const {
  for (size_t i = 0; i < _mainDirs.size(); ++i) {
    if (&_mainDirs[i] == a) return i;
  }

  return _mainDirs.size();
}

// _____________________________________________________________________________
void Node::addRouteConnException(const Route* r, const Edge* edgeA, const Edge* edgeB) {
  _routeConnExceptions[r][edgeA].insert(edgeB);
  // index the other direction also, will lead to faster lookups later on
  _routeConnExceptions[r][edgeB].insert(edgeA);
}

// _____________________________________________________________________________
bool Node::connOccurs(const Route* r, const Edge* edgeA, const Edge* edgeB)
const {
  const auto& i = _routeConnExceptions.find(r);
  if (i == _routeConnExceptions.end()) return true;
  const auto& ii = i->second.find(edgeA);
  if (ii == i->second.end()) return true;

  return ii->second.find(edgeB) == ii->second.end();
}

// _____________________________________________________________________________
Edge* Node::getEdge(const Node* other) const {
  for (auto e = _adjListOut.begin(); e != _adjListOut.end(); ++e) {
    Edge* eP = *e;

    if (eP->getTo() == other) {
      return eP;
    }
  }

  for (auto e = _adjListIn.begin(); e != _adjListIn.end(); ++e) {
    Edge* eP = *e;

    if (eP->getFrom() == other) {
      return eP;
    }
  }
}
