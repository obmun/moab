/*
 * MOAB, a Mesh-Oriented datABase, is a software component for creating,
 * storing and accessing finite element mesh data.
 * 
 * Copyright 2004 Sandia Corporation.  Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Coroporation, the U.S. Government
 * retains certain rights in this software.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 */

/**\file MBOrientedBox.hpp
 *\author Jason Kraftcheck (kraftche@cae.wisc.edu)
 *\date 2006-07-18
 */

#ifndef MB_ORIENTED_BOX_HPP
#define MB_ORIENTED_BOX_HPP

#include "MBInterface.hpp"
#include "MBCartVect.hpp"

#include <iosfwd>

#define MB_ORIENTED_BOX_UNIT_VECTORS 1
#define MB_ORIENTED_BOX_OUTER_RADIUS 1

class MBRange;


/**\brief Oriented bounding box
 */
struct MBOrientedBox
{
  MBCartVect center;  //!< Box center
  MBCartVect axis[3]; //!< Box axes, unit vectors sorted by extent of box along axis
#if MB_ORIENTED_BOX_UNIT_VECTORS
  MBCartVect length;  //!< distance from center to plane along each axis
#endif
#if MB_ORIENTED_BOX_OUTER_RADIUS
  double radius;
#endif

  inline double inner_radius() const; // radius of inscribed sphere
  inline double outer_radius() const; // radius of circumscribed sphere
  inline double outer_radius_squared() const;
  inline double volume() const;
  inline MBCartVect dimensions() const;
  inline double area() const; // max of area of sides of box
  
  bool contained( const MBCartVect& point, double tolerance ) const;
  
  //bool contained( const MBOrientedBox& other, double tolerance ) const;
  
  /**\brief get tag handle for storing oriented box
   *
   * Get the handle for the tag with the specified name and
   * check that the tag is appropriate for storing instances
   * of MBOrientedBox.  The resulting tag may be used to store
   * instances of MBOrientedBox directly.
   *
   *\param handle_out  The TagHandle, passed back to caller
   *\param name        The tag name
   *\param create      If true, tag will be created if it does not exist
   */
  static MBErrorCode tag_handle( MBTag& handle_out,
                                 MBInterface* instance, 
                                 const char* name,
                                 bool create = true );

  /**\brief Calculate an oriented box from a set of vertices */
  static MBErrorCode compute_from_vertices( MBOrientedBox& result,
                                            MBInterface* instance,
                                            const MBRange& vertices );
                                  
  /**\brief Calculate an oriented box from a set of 2D elements */
  static MBErrorCode compute_from_2d_cells( MBOrientedBox& result,
                                            MBInterface* instance,
                                            const MBRange& elements );
                                    
};

std::ostream& operator<<( std::ostream&, const MBOrientedBox& );

double MBOrientedBox::inner_radius() const
{
#if MB_ORIENTED_BOX_UNIT_VECTORS
  return length[0];
#else
  return axis[0].length();
#endif
}

double MBOrientedBox::outer_radius() const
{
#if MB_ORIENTED_BOX_OUTER_RADIUS
  return radius;
#elif MB_ORIENTED_BOX_UNIT_VECTORS
  return length.length();
#else
  return (axis[0] + axis[1] + axis[2]).length();
#endif
}

double MBOrientedBox::outer_radius_squared() const
{
#if MB_ORIENTED_BOX_OUTER_RADIUS
  return radius * radius;
#elif MB_ORIENTED_BOX_UNIT_VECTORS
  return length % length;
#else
  const MBCartVect half_diag = axis[0] + axis[1] + axis[2];
  return half_diag % half_diag;
#endif
}

double MBOrientedBox::volume() const
{
#if MB_ORIENTED_BOX_UNIT_VECTORS
  return 8 * length[0] * length[1] * length[2];
#else
  return fabs(8 * axis[0] % (axis[1] * axis[2]));
#endif
}

MBCartVect MBOrientedBox::dimensions() const
{
#if MB_ORIENTED_BOX_UNIT_VECTORS
  return 2.0 * length;
#else
  return 2.0 * MBCartVect( axis[0].length(), axis[1].length(), axis[2].length() );
#endif
}

double MBOrientedBox::area() const
{
#if MB_ORIENTED_BOX_UNIT_VECTORS
  return 4 * length[1] * length[2];
#else
  return 4 * (axis[1] * axis[2]).length();
#endif
}

#endif
