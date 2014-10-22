#ifndef TEMPLATES_HPP
#define TEMPLATES_HPP

#include "NestedRefine.hpp"

namespace moab{

  const NestedRefine::refPatterns NestedRefine::refTemplates[9][MAX_DEGREE]=
  {
    //EDGE
    {
      // Deg 2
      /*    0------2------1    */
     /*           1            2         */

      {1,0,0,1,2,
       {2,2},
       {{1.0/2.0,0,0}},
       {{0,2},{2,1}},
       {{1,0},{2,0},{0,0}},
       {{0,0,2,0}, {1,1,0,0}},
       {},
       {},
       {{1,1},{1,2}}},

      // Deg 3
      /*    0------2------3------1    */
     /*           1            2           3         */

      {2,0,0,2,3,
       {2,3},
       {{1.0/3.0,0,0},{2.0/3.0,0,0}},
       {{0,2},{2,3},{3,1}},
       {{1,0},{2,0},{3,0},{0,0}},
       {{0,0,2,0},{1,1,3,0},{2,1,0,0}},
       {},
       {},
       {{1,1},{1,3}}},

      // Deg 5
      /*    0------2------3------4------5------1    */
     /*           1            2          3           4            5         */

      {4,0,0,4,5,
       {2,5},
       {{1.0/5.0,0,0},{2.0/5.0,0,0},{3.0/5.0,0,0},{4.0/5.0,0,0}},
       {{0,2},{2,3},{3,4},{4,5},{5,1}},
       {{1,0},{2,0},{3,0},{4,0},{5,0},{0,0}},
       {{0,0,2,0},{1,1,3,0},{2,1,4,0},{3,1,5,0},{4,1,0,0}},
       {},
       {},
       {{1,1},{1,5}}}
    }

    //TRI
//   {
      // Deg 2
      /*  2
         /  \
     5 /-- \ 4
       /_\/_\
      0   3  1   */
/*
      {1,0,0,3,4,
       {3,5},
       {{0.5,0,0},{0.5,0.5,0},{0,0.5,0}},
       {{3},{4},{5}},
       {},
       {{3,0},{4,1},{1,2}},
       {{0,3,5},{3,4,5},{3,1,4},{5,4,2}},
       {{0,0,2,2,0,0},{3,2,4,0,1,1},{0,0,0,0,2,0},{2,1,0,0,0,0}},
       2, {{1,0,3,0},{3,1,4,1},{4,2,1,2}}
       },
*/
      // Deg 3
      /*  2
          /_\
        7    6
       /_\/_\
      8   9    5
     /_\/_\/_\
    0   3   4   1
    */

/*      {2,1,0,7,9,
       {3,9},
       {{1.0/3.0,0,0},{2.0/3.0,0,0},{2.0/3.0,1.0/3.0,0},{1.0/3.0,2.0/3.0,0},{0,2.0/3.0,0},{0,1.0/3.0,0},{1.0/3.0,1.0/3.0,0}},

       {{3,4},{5,6},{7,8}},
       {9},
       {{3,0},{5,0},{8,1},{9,1},{6,2},{1,2},{8,0}},

       {{0,3,8},{3,9,8},{3,4,9},{4,5,9},{4,1,5},{8,9,7},{9,6,7},{9,5,6},{7,6,2}},
       {{0,0,2,2,0,0},{3,0,6,0,1,1},{0,0,4,2,2,1},{5,2,8,0,3,1},{0,0,0,0,4,0},{2,1,7,2,0,0},{8,2,9,0,6,1},{4,1,0,0,7,0},{7,2,0,0,0,0}},

       3, {{1,0,3,0,5,0},{5,1,8,1,9,1},{9,2,6,2,1,2}},
       },
*/
      // Deg 5
      /*    2
            /_\
          11  10
          /_\/_\
        12 19  9
        /_\/_\/_\
      13 20 18  8
      /_\/_\/_\/_\
    14 15 16 17  7
    /_\/_\/_\/_\/_\
   0   3   4   5   6   1
    */

/*
     {4,6,0,18,25,
      {3,20},
      {{1.0/5.0,0,0},{2.0/5.0,0,0},{3.0/5.0,0,0},{4.0/5.0,0,0},{4.0/5.0,1.0/5.0,0},{3.0/5.0,2.0/5.0,0},{2.0/5.0,3.0/5.0,0},{1.0/5.0,4.0/5.0,0},{0,4.0/5.0,0},{0,3.0/5.0,0},{0,1.0/5.0,0},{1.0/5.0,1.0/5.0,0},{2.0/5.0,1.0/5.0,0},{3.0/5.0,1.0/5.0,0},{2.0/5.0,2.0/5.0,0},{1.0/5.0,3.0/5.0,0},{1.0/5.0,3.0/5.0,0}},

     {{3,4,5,6},{7,8,9,10},{11,12,13,14}},
      {15,16,17,18,19,20},
      {},

     {{0,3,14},{3,15,14},{3,4,15},{4,16,15},{4,5,16},{5,17,16},{5,6,17},{6,7,17},{6,1,7},{14,15,13},{15,20,13},{15,16,20},{16,18,20},{16,17,18},{17,8,18},{17,7,8},{13,20,12},{20,19,12},{20,18,19},{18,9,19},{18,8,9},{12,19,11},{19,10,11},{19,9,10},{11,10,2}},

     {{0,0,2,2,0,0},{3,2,10,0,1,1},{0,0,4,2,2,0},{5,2,12,0,3,1},{0,0,6,2,4,1},{7,2,14,0,5,1},{0,0,8,2,6,1},{9,2,16,0,7,1},{0,0,0,0,8,1},{2,1,11,2,0,0},{12,2,17,0,10,1},{4,1,13,2,11,0},{14,2,19,0,12,1},{6,1,15,2,13,1},{16,2,21,0,14,1},{8,1,0,0,15,0},{11,1,18,2,0,0},{19,2,22,0,17,1},{13,1,20,2,18,0},{21,2,24,0,19,1},{15,1,0,0,20,0},{18,1,23,2,0,0},{24,2,25,0,22,1},{20,1,0,0,23,0},{23,1,0,0,0,0}},

     5, {{1,0,3,0,5,0,7,0,9,0},{9,1,16,1,21,1,24,1,25,1},{25,2,22,2,17,2,10,2,1,2}}
      }

    }, */
/*
    //QUAD
    {
      {1,1,0,5,4,{4,8}, {{0,-1,0},{1,0,0},{0,1,0},{-1,0,0},{0,0,0}}, {{4},{5},{6},{7}}, {8}, {{0,4,8,7},{4,1,5,8},{8,5,2,6},{7,8,6,3}}, {{0,0,2,3,4,0,0,0},{0,0,0,0,3,0,1,1},{2,2,0,0,0,0,4,1},{1,2,3,3,0,0,0,0}}, {{1,0,2,0},{2,1,3,1},{3,2,4,2},{4,3,1,3}}, 4}, // deg 2

      {2,4,0,12,9,{4,15}, {{-1/3,-1,0},{1/3,-1,0},{1,-1/3,0},{1,1/3,0},{1/3,1,0},{-1/3,1,0},{-1,1/3,0},{-1,-1/3,0},{-1/3, -1/3,0},{1/3, -1/3,0},{1/3, 1/3,0},{-1/3, 1/3,0}},
      {{4,5},{6,7},{8,9},{10,11}}, {12,13,14,15}, {{0,4,12,11},{4,5,13,12},{5,1,6,13},{11,12,15,10},{12,13,14,15},{13,6,7,14},{10,15,9,3},{15,14,8,9},{14,7,2,8}},
       {{0,0,2,3,4,0,0,0}, {0,0,3,3,5,0,1,1}, {0,0,0,0,6,0,2,1}, {1,2,5,3,7,0,0,0}, {2,2,6,3,8,0,4,1}, {3,2,0,0,9,0,5,1}, {4,2,8,3,0,0,0,0}, {5,2,9,3,0,0,7,1}, {6,2,0,0,0,0,8,1}},
      {{1,0,2,0,3,0},{3,1,6,1,9,1},{9,2,8,2,7,2},{7,3,4,3,1,3}}, 9}, // deg 3

      {4,16,0,32,25,{4,35},
       {{-3/5,-1,0},{-1/5,-1,0},{1/5,-1,0},{3/5,-1,0},{1,-3/5,0},{1,-1/5,0},{1,1/5,0},{1,3/5,0},{3/5,1,0},{1/5,1,0},{-1/5,1,0},{-3/5,1,0},{-1,3/5,0},{-1,1/5,0},{-1,-1/5,0},{-1,-3/5,0},{-3/5,-3/5,0},{-1/5,-3/5,0},{1/5,-3/5,0},{3/5,-3/5,0},{3/5,-1/5,0},{3/5,1/5,0},{3/5,3/5,0},{1/5,3/5,0},{-1/5,3/5,0},{-3/5,3/5,0},{-3/5,1/5,0},{-3/5,-1/5,0},{-1/5,-1/5,0},{1/5,-1/5,0},{1/5,1/5,0},{-1/5,1/5,0}},
      {{0,4,20,19},{4,5,21,20},{5,6,22,21},{6,7,23,22},{7,1,8,23},{19,20,31,18},{20,21,32,31},{21,22,33,32},{22,23,24,33},{23,8,9,24},{18,31,30,17},{31,32,35,30},{32,33,34,35},{33,24,25,34},{24,9,10,25},{17,30,29,16},{30,35,28,29},{35,34,27,28},{34,25,26,27},{25,10,11,26},{16,29,15,3},{29,28,14,15},{28,27,13,14},{27,26,12,13},{26,11,2,12}},
      {{0,0,2,3,6,0,0,0},{0,0,3,3,7,0,1,1},{0,0,4,3,8,0,2,1},{0,0,5,3,9,0,3,1},{0,0,0,0,10,0,4,1},{1,2,7,3,11,0,0,0},{2,2,8,3,12,0,6,1},{3,2,9,3,13,0,7,1},{4,2,10,3,14,0,8,1},{5,2,0,0,15,0,9,1},{6,2,12,3,16,0,0,0},{7,2,13,3,17,0,11,1},{8,2,14,3,18,0,12,1},{9,2,15,3,9,0,13,1},{10,2,0,0,20,0,14,1},{11,2,17,3,21,0,0,0},{12,2,18,3,22,0,16,1},{13,2,19,3,23,0,17,1},{14,2,20,3,24,0,18,1},{15,2,0,0,25,0,19,1},{16,2,22,3,0,0,0,0},{17,2,23,3,0,0,21,1},{18,2,24,3,0,0,22,1},{19,2,25,3,0,0,23,1},{20,2,0,0,0,0,24,1}},
      {{1,0,2,0,3,0,4,0,5,0},{5,1,10,1,15,1,20,1,25,1},{25,2,24,2,23,2,22,2,21,2},{21,3,16,3,11,3,6,3,1,3}}, 25} // deg5
    },

    //POLYGON
    {{0,0,0,0,0,{},{{}},{{}},{{}},{{}},{{}},{{}},0},
     {0,0,0,0,0,{},{{}},{{}},{{}},{{}},{{}},{{}},0}},
    // TET
    {
      {1,0,0,6,8,{4,9},{{1/2,0,0},{1/2,1/2,0},{0,1/2,0},{0,0,1/2},{1/2,0,1/2},{0,1/2,1/2}}, {{4},{5},{6},{7},{8},{9}}, {}, {{0,4,6,7},{5,9,6,4,8,7},{4,1,5,8},{6,5,2,9},{7,8,9,3}},
       {{0,0,2,5,0,0,0,0},{0,0,5,3,4,0,0,0,0,0,1,1,3,2,0,0},{0,0,0,0,2,6,0,0},{2,2,0,0,0,0,0,0},{0,0,0,0,0,0,2,1}}, {{1,0,2,7,3,0,5,0},{3,1,2,1,4,1,5,1},{4,2,2,3,1,2,5,2},{1,3,2,4,4,3,3,3}}, 5}, //deg 2
      {{0,0,0,0,0,{},{{}},{{}},{{}},{{}},{{}},{{}},0}} // deg3

    },
    //PYRAMID
    {{0,0,0,0,0,{},{{}},{{}},{{}},{{}},{{}},{{}},0},
     {0,0,0,0,0,{},{{}},{{}},{{}},{{}},{{}},{{}},0}},
    //PRISM
    {
      {1,1,0,12,8,{6,17}, {{1/2,0,-1},{1/2,1/2,-1},{0,1/2,-1},{0,0,0},{1,0,0},{0,1,0},{1/2,0,1},{1/2,1/2,1},{0,1/2,1},{1/2,0,0},{1/2,1/2,0},{0,1/2,0}},
      {{6},{7},{8},{9},{10},{11},{12},{13},{14}}, {{15},{16},{17},{},{}},
      {{0,6,8,9,15,17},{6,7,8,15,16,17},{6,1,7,15,10,16},{8,7,2,17,16,11},{9,15,17,3,12,14},{15,16,17,12,13,14},{15,10,16,12,4,13},{17,16,11,14,13,5}},
      {{0,0,2,2,0,0,0,0,5,3},{3,2,4,0,1,1,0,0,6,3},{0,0,0,0,2,0,0,0,7,3},{2,1,0,0,0,0,0,0,8,3},{0,0,6,2,0,0,1,4,0,0},{7,2,8,0,5,1,2,4,0,0},{0,0,0,0,6,0,3,4,0,0},{6,1,0,0,0,0,4,4,0,0}},
      {{1,0,3,0,5,0,7,0},{3,1,4,1,7,1,8,1},{1,2,4,2,5,2,8,2},{1,3,2,3,3,3,4,3},{5,4,6,4,7,4,8,4}}, 8}, //deg 2

      {0,0,0,0,0,{},{{}},{{}},{{}},{{}},{{}},{{}},0} //deg3
    }
    //KNIFE
   // {{},{}},
    //HEX
  //  {{},{}}*/
  };

  const NestedRefine::tessellate_octahedron NestedRefine::oct_tessellation[3] =
  {
    {1, {0,5}, {{0,1,5,4},{0,1,2,5},{2,0,5,3},{3,5,4,0}}, {{0,0,0,0,4,1,2,0},{1,3,0,0,3,3,0,0},{0,0,4,2,0,0,2,2},{0,0,1,2,3,1,0,0}}, {{1,0},{1,1},{2,3},{2,1},{3,0},{3,2},{4,0},{4,3}}, {{0,1,-1,-1},{-1,3,-1,2},{4,-1,5,-1},{-1,-1,6,7}} },

    {2, {1,3}, {{0,4,1,3},{4,5,1,3},{0,1,2,3},{1,5,2,3}}, {{0,0,2,2,3,0,0,0},{0,0,4,0,1,1,0,0},{1,2,4,2,0,0,0,0},{2,1,0,0,3,1,0,0}}, {{1,3},{2,3},{3,3},{4,3},{3,2},{4,1},{1,0},{2,0}}, {{6,-1,-1,0},{7,-1,-1,1},{-1,-1,4,2},{-1,5,-1,3}} },

    {3, {2,4}, {{0,4,1,2},{1,4,5,2},{3,5,4,2},{3,4,0,2}}, {{4,2,0,0,2,0,0,0},{1,2,3,2,0,0,0,0},{0,0,4,0,2,1,0,0},{3,1,0,0,1,0,0,0}}, {{1,3},{2,3},{1,2},{2,2},{4,2},{3,0},{4,3},{3,3}}, {{-1,-1,2,0},{-1,-1,3,1},{5,-1,-1,7},{-1,4,-1,6}} }
  };

  const NestedRefine::pmat NestedRefine::permute_matrix[2] =
  {
    {6, {{0,1,2},{0,2,1},{1,0,2},{2,1,0},{1,2,0},{2,0,1}}}, //TRI
    {8, {{0,1,2,3},{1,2,3,0},{2,3,0,1},{3,0,1,2},{1,0,3,2},{0,3,2,1},{3,2,1,0},{2,1,0,3}}} //QUAD
  };

}//namesapce moab

#endif
