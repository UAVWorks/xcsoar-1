/* Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2010 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
 */
#ifndef DIJKSTRA_HPP
#define DIJKSTRA_HPP

#include <map>
#include "Util/queue.hpp"
#include <assert.h>

#ifdef INSTRUMENT_TASK
extern long count_dijkstra_links;
#endif

#define MINMAX_OFFSET 134217727

//uncomment this line to reserve space in queue
#define USE_RESERVABLE
#ifdef USE_RESERVABLE
#define DIJKSTRA_QUEUE_SIZE 50000
#endif

/**
 * Dijkstra search algorithm.
 * Modifications by John Wharington to track optimal solution
 * @see http://en.giswiki.net/wiki/Dijkstra%27s_algorithm
 */
template <class Node> class Dijkstra {
public:
  /**
   * Default constructor
   *
   * @param is_min Whether this algorithm will search for min or max distance
   */
  Dijkstra(const bool is_min = true) :
    m_min(is_min) {
    reserve();
  }

  /**
   * Constructor
   *
   * @param n Node to start
   * @param is_min Whether this algorithm will search for min or max distance
   */
  Dijkstra(const Node &node, const bool is_min = true) :
    m_min(is_min) { 
    push(node, node, 0);
    reserve();
  }

  /**
   * Resets as if constructed afresh
   *
   * @param n Node to start
   */
  void restart(const Node &node) {
    clear();
    push(node, node, 0);
  }

  /** 
   * Clears the queues
   */
  void clear() {
    // Clear the search queue
    while (!q.empty())
      q.pop();

    // Clear the node_parent_map
    node_parents.clear();
    // Clear the node_value_map
    node_values.clear();
  }

  /**
   * Test whether queue is empty
   *
   * @return True if no more nodes to search
   */
  bool empty() const {
    return q.empty();
  }

  /**
   * Return size of queue
   *
   * @return Queue size in elements
   */
  unsigned queue_size() const {
    return q.size();
  }

  /**
   * Return top element of queue for processing
   *
   * @return Node for processing
   */
  const Node &pop() {
    cur = q.top().second;

    do
      q.pop();
    while (!q.empty() && q.top().second->second < q.top().first);

    return cur->first;
  }

  /**
   * Add an edge (node-node-distance) to the search
   *
   * @param n Destination node to add
   * @param pn Predecessor of destination node
   * @param e Edge distance
   */
  void link(const Node &node, const Node &parent, const unsigned &edge_value = 1) {
#ifdef INSTRUMENT_TASK
    count_dijkstra_links++;
#endif
    push(node, parent, cur->second + adjust_edge_value(edge_value)); 
  }

  /**
   * Find best predecessor found so far to the specified node
   *
   * @param n Node as destination to find best predecessor for
   *
   * @return Predecessor node
   */
  Node get_predecessor(const Node &node) const {
    // Try to find the given node in the node_parent_map
    node_parent_const_iterator it = node_parents.find(node);
    if (it == node_parents.end())
      // first entry
      // If the node wasn't found
      // -> Return the given node itself
      return node;
    else
      // If the node was found
      // -> Return the parent node
      return (it->second); 
  }

private:

  void reserve() {
#ifdef USE_RESERVABLE
    q.reserve(DIJKSTRA_QUEUE_SIZE);
#endif
  }

  /** 
   * Return edge value adjusted for flipping if maximim is sought ---
   * result is metric to be minimised
   */
  unsigned adjust_edge_value(const unsigned edge_value) const {
    return m_min ? edge_value : MINMAX_OFFSET - edge_value;
  }

  /**
   * Add node to search queue
   *
   * @param n Destination node to add
   * @param pn Previous node
   * @param e Edge distance (previous to this)
   */
  void push(const Node &node, const Node &parent, const unsigned &edge_value = 0) {
    // Try to find the given node n in the node_value_map
    node_value_iterator it = node_values.find(node);
    if (it == node_values.end()) {
      // first entry
      // If the node wasn't found
      // -> Insert a new node into the node_value_map
      it = node_values.insert(make_pair(node, edge_value)).first;

      // Remember the parent node
      set_predecessor(node, parent);
    } else if (it->second > edge_value) {
      // If the node was found and the value is smaller
      // -> Replace the value with the new one
      it->second = edge_value;
      // replace, it's bigger

      // Remember the new parent node
      set_predecessor(node, parent);
    } else
      // If the node was found but the value is higher or equal
      // -> Don't use this new leg
      return;

    q.push(make_pair(edge_value, it));
  }

  void set_predecessor(const Node &node, const Node &parent) {
    // Try to find the given node in the node_parent_map
    node_parent_iterator it = node_parents.find(node);
    if (it == node_parents.end())
      // first entry
      // If the node wasn't found
      // -> Insert a new node into the node_parent_map
      node_parents.insert(make_pair(node, parent));
    else
      // If the node was found
      // -> Replace the according parent node with the new one
      it->second = parent; 
  }

  typedef std::map<Node, unsigned> node_value_map;
  typedef typename node_value_map::iterator node_value_iterator;

  typedef std::map<Node, Node> node_parent_map;
  typedef typename node_parent_map::iterator node_parent_iterator;
  typedef typename node_parent_map::const_iterator node_parent_const_iterator;

  typedef std::pair<unsigned, node_value_iterator> Value;

  struct Rank : public std::binary_function<Value, Value, bool> {
    bool operator()(const Value& x, const Value& y) const {
      return x.first > y.first;
    }
  };

  /**
   * Stores the value of each node.  It is updated by push(), if a
   * value lower than the current one is found.
   */
  node_value_map node_values;

  /**
   * Stores the predecessor of each node.  It is maintained by
   * set_predecessor().
   */
  node_parent_map node_parents;

  /**
   * A sorted list of all possible node paths, lowest distance first.
   */
#ifdef USE_RESERVABLE
  reservable_priority_queue<Value, std::vector<Value>, Rank> q;
#else
  std::priority_queue<Value, std::vector<Value>, Rank> q;
#endif

  node_value_iterator cur;
  const bool m_min;
};

#endif
