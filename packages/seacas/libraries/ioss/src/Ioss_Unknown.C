// Copyright(C) 1999-2010
// Sandia Corporation. Under the terms of Contract
// DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
// certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Sandia Corporation nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "Ioss_CodeTypes.h"           // for IntVector
#include "Ioss_ElementTopology.h"     // for ElementTopology
#include <Ioss_ElementVariableType.h> // for ElementVariableType
#include <Ioss_Unknown.h>
#include <cassert> // for assert

// ========================================================================
namespace Ioss {
  class St_Unknown : public ElementVariableType
  {
  public:
    static void factory();

  protected:
    St_Unknown() : ElementVariableType("unknown", 0) {}
  };
} // namespace Ioss
void Ioss::St_Unknown::factory() { static Ioss::St_Unknown registerThis; }

// ========================================================================
namespace {
  struct Constants
  {
    static const int nnode     = 0;
    static const int nedge     = 0;
    static const int nedgenode = 0;
    static const int nface     = 0;
    static const int nfacenode = 0;
  };
} // namespace

void Ioss::Unknown::factory()
{
  static Ioss::Unknown registerThis;
  Ioss::St_Unknown::factory();
}

Ioss::Unknown::Unknown() : Ioss::ElementTopology("unknown", "unknown")
{
  Ioss::ElementTopology::alias("unknown", "invalid_topology");
}

Ioss::Unknown::~Unknown() = default;

int Ioss::Unknown::parametric_dimension() const { return 0; }
int Ioss::Unknown::spatial_dimension() const { return 0; }
int Ioss::Unknown::order() const { return 0; }

int Ioss::Unknown::number_corner_nodes() const { return number_nodes(); }
int Ioss::Unknown::number_nodes() const { return Constants::nnode; }
int Ioss::Unknown::number_edges() const { return Constants::nedge; }
int Ioss::Unknown::number_faces() const { return Constants::nface; }

int Ioss::Unknown::number_nodes_edge(int /* edge */) const { return Constants::nedgenode; }

int Ioss::Unknown::number_nodes_face(int face) const
{
  // face is 1-based.  0 passed in for all faces.
  assert(face >= 0 && face <= number_faces());
  return Constants::nfacenode;
}

int Ioss::Unknown::number_edges_face(int face) const
{
  // face is 1-based.  0 passed in for all faces.
  assert(face >= 0 && face <= number_faces());
  return Constants::nfacenode;
}

Ioss::IntVector Ioss::Unknown::edge_connectivity(int edge_number) const
{
  Ioss::IntVector connectivity;
  assert(edge_number >= 0 && edge_number <= Constants::nedge);
  return connectivity;
}

Ioss::IntVector Ioss::Unknown::face_connectivity(int face_number) const
{
  assert(face_number >= 0 && face_number <= number_faces());
  Ioss::IntVector connectivity;
  return connectivity;
}

Ioss::IntVector Ioss::Unknown::element_connectivity() const
{
  Ioss::IntVector connectivity;
  return connectivity;
}

Ioss::ElementTopology *Ioss::Unknown::face_type(int face_number) const
{
  // face_number == 0 returns topology for all faces if
  // all faces are the same topology; otherwise, returns nullptr
  // face_number is 1-based.

  assert(face_number >= 0 && face_number <= number_faces());
  return Ioss::ElementTopology::factory("unknown");
}

Ioss::ElementTopology *Ioss::Unknown::edge_type(int edge_number) const
{
  // edge_number == 0 returns topology for all edges if
  // all edges are the same topology; otherwise, returns nullptr
  // edge_number is 1-based.

  assert(edge_number >= 0 && edge_number <= number_edges());
  return Ioss::ElementTopology::factory("unknown");
}
