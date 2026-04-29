// Scenario data factories.
//
// Each multiple_survey_<n>() returns a fully-populated Data<D>
// describing one of the structured "multiple survey" scenarios used
// by the per-scenario benchmark binaries.  Data definitions are the
// NDEBUG (full-size) variants extracted from the original monolithic
// benchmark.cpp.
//
#pragma once

#include <benchmark/Common.hpp>

namespace
{
  [[nodiscard]] inline auto multiple_survey_2_l() -> Data<3>
  {
    return Data<3>
              {
                Grid<3>
                  { Origin<3> {0, 0, 0}
                  , Extents<3> {360, 160, 1}
                  , Strides<3> {1, 3, 1}
                  }
              , Grid<3>
                  { Origin<3> {0, 160, 0}
                  , Extents<3> {60, 240, 1}
                  , Strides<3> {3, 7, 1}
                  }
              };
  }

  [[nodiscard]] inline auto multiple_survey_3_steps() -> Data<3>
  {
    return Data<3>
              {
                Grid<3>
                  { Origin<3> {0, 0, 0}
                  , Extents<3> {360, 240, 1}
                  , Strides<3> {1, 1, 1}
                  }
              , Grid<3>
                  { Origin<3> {320, 60, 0}
                  , Extents<3> {200, 280, 1}
                  , Strides<3> {3, 5, 1}
                  }
              , Grid<3>
                  { Origin<3> {160, 360, 0}
                  , Extents<3> {320, 160, 1}
                  , Strides<3> {7, 2, 1}
                  }
              };
  }

  [[nodiscard]] inline auto multiple_survey_4_overlap() -> Data<3>
  {
    return Data<3>
              {
                Grid<3>
                  { Origin<3> {0, 0, 0}
                  , Extents<3> {400, 450, 1}
                  , Strides<3> {1, 1, 1}
                  }
              , Grid<3>
                  { Origin<3> {240, 180, 0}
                  , Extents<3> {440, 360, 1}
                  , Strides<3> {2, 3, 1}
                  }
              , Grid<3>
                  { Origin<3> {120, 600, 0}
                  , Extents<3> {360, 300, 1}
                  , Strides<3> {5, 1, 1}
                  }
              , Grid<3>
                  { Origin<3> {560, 0, 0}
                  , Extents<3> {240, 660, 1}
                  , Strides<3> {3, 7, 1}
                  }
              };
  }

  [[nodiscard]] inline auto multiple_survey_5_mixed() -> Data<3>
  {
    return Data<3>
              {
                Grid<3>
                  { Origin<3> {0, 0, 0}
                  , Extents<3> {440, 660, 1}
                  , Strides<3> {1, 1, 1}
                  }
              , Grid<3>
                  { Origin<3> {200, 300, 0}
                  , Extents<3> {320, 540, 1}
                  , Strides<3> {3, 7, 1}
                  }
              , Grid<3>
                  { Origin<3> {560, 120, 0}
                  , Extents<3> {300, 480, 1}
                  , Strides<3> {7, 3, 1}
                  }
              , Grid<3>
                  { Origin<3> {80, 900, 0}
                  , Extents<3> {520, 360, 1}
                  , Strides<3> {5, 9, 1}
                  }
              , Grid<3>
                  { Origin<3> {720, 660, 0}
                  , Extents<3> {280, 420, 1}
                  , Strides<3> {10, 3, 1}
                  }
              };
  }

  [[nodiscard]] inline auto multiple_survey_6_bands() -> Data<3>
  {
    return Data<3>
              {
                Grid<3>
                  { Origin<3> {0, 0, 0}
                  , Extents<3> {640, 360, 1}
                  , Strides<3> {1, 1, 1}
                  }
              , Grid<3>
                  { Origin<3> {520, 80, 0}
                  , Extents<3> {520, 360, 1}
                  , Strides<3> {2, 3, 1}
                  }
              , Grid<3>
                  { Origin<3> {160, 440, 0}
                  , Extents<3> {520, 300, 1}
                  , Strides<3> {5, 7, 1}
                  }
              , Grid<3>
                  { Origin<3> {760, 440, 0}
                  , Extents<3> {440, 320, 1}
                  , Strides<3> {9, 4, 1}
                  }
              , Grid<3>
                  { Origin<3> {0, 840, 0}
                  , Extents<3> {600, 240, 1}
                  , Strides<3> {4, 9, 1}
                  }
              , Grid<3>
                  { Origin<3> {640, 840, 0}
                  , Extents<3> {480, 240, 1}
                  , Strides<3> {7, 5, 1}
                  }
              };
  }

  [[nodiscard]] inline auto multiple_survey_7_large() -> Data<3>
  {
    return Data<3>
              {
                Grid<3>
                  { Origin<3> {0, 0, 0}
                  , Extents<3> {560, 440, 1}
                  , Strides<3> {1, 1, 1}
                  }
              , Grid<3>
                  { Origin<3> {440, 80, 0}
                  , Extents<3> {520, 440, 1}
                  , Strides<3> {3, 5, 1}
                  }
              , Grid<3>
                  { Origin<3> {80, 480, 0}
                  , Extents<3> {520, 360, 1}
                  , Strides<3> {5, 3, 1}
                  }
              , Grid<3>
                  { Origin<3> {640, 520, 0}
                  , Extents<3> {440, 360, 1}
                  , Strides<3> {7, 9, 1}
                  }
              , Grid<3>
                  { Origin<3> {160, 920, 0}
                  , Extents<3> {480, 320, 1}
                  , Strides<3> {9, 7, 1}
                  }
              , Grid<3>
                  { Origin<3> {720, 960, 0}
                  , Extents<3> {440, 280, 1}
                  , Strides<3> {10, 1, 1}
                  }
              , Grid<3>
                  { Origin<3> {320, 280, 0}
                  , Extents<3> {520, 520, 1}
                  , Strides<3> {2, 7, 1}
                  }
              };
  }

  [[nodiscard]] inline auto multiple_survey_8_threed() -> Data<3>
  {
    return Data<3>
              {
                // Row 0
                Grid<3> { Origin<3> {  0,   0, 0}, Extents<3> {30, 25, 10}, Strides<3> {2, 3, 4} }
              , Grid<3> { Origin<3> { 52,   0, 0}, Extents<3> {30, 25, 10}, Strides<3> {3, 3, 5} }
              , Grid<3> { Origin<3> {132,   0, 0}, Extents<3> {30, 25, 10}, Strides<3> {2, 3, 3} }
                // Row 1 (y-origin 66, slight y-overlap with row 0 which ends at 72)
              , Grid<3> { Origin<3> {  0,  66, 0}, Extents<3> {30, 25, 10}, Strides<3> {3, 2, 5} }
              , Grid<3> { Origin<3> { 52,  66, 0}, Extents<3> {30, 25, 10}, Strides<3> {2, 2, 4} }
              , Grid<3> { Origin<3> {132,  66, 0}, Extents<3> {30, 25, 10}, Strides<3> {3, 2, 3} }
                // Row 2 (y-origin 108, slight y-overlap with row 1 which ends at 114)
              , Grid<3> { Origin<3> {  0, 108, 0}, Extents<3> {30, 25, 10}, Strides<3> {2, 3, 3} }
              , Grid<3> { Origin<3> { 52, 108, 0}, Extents<3> {30, 25, 10}, Strides<3> {3, 3, 4} }
              , Grid<3> { Origin<3> {132, 108, 0}, Extents<3> {30, 25, 10}, Strides<3> {2, 3, 5} }
                // Row 3 (y-origin 174, slight y-overlap with row 2 which ends at 180)
              , Grid<3> { Origin<3> {  0, 174, 0}, Extents<3> {30, 25, 10}, Strides<3> {3, 2, 4} }
              , Grid<3> { Origin<3> { 52, 174, 0}, Extents<3> {30, 25, 10}, Strides<3> {2, 2, 3} }
              , Grid<3> { Origin<3> {132, 174, 0}, Extents<3> {30, 25, 10}, Strides<3> {3, 2, 5} }
              };
  }

  [[nodiscard]] inline auto multiple_survey_9_5d() -> Data<5>
  {
    return Data<5>
              {
                // Row 0
                Grid<5> { Origin<5> { 0,  0, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {2, 3, 4, 1, 1} }
              , Grid<5> { Origin<5> { 8,  0, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {3, 3, 5, 1, 1} }
              , Grid<5> { Origin<5> {19,  0, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {2, 3, 3, 1, 1} }
                // Row 1 (y-origin 9, slight y-overlap with row 0)
              , Grid<5> { Origin<5> { 0,  9, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {3, 2, 5, 1, 1} }
              , Grid<5> { Origin<5> { 8,  9, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {2, 2, 4, 1, 1} }
              , Grid<5> { Origin<5> {19,  9, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {3, 2, 3, 1, 1} }
                // Row 2 (y-origin 16, slight y-overlap with row 1)
              , Grid<5> { Origin<5> { 0, 16, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {2, 3, 3, 1, 1} }
              , Grid<5> { Origin<5> { 8, 16, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {3, 3, 4, 1, 1} }
              , Grid<5> { Origin<5> {19, 16, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {2, 3, 5, 1, 1} }
                // Row 3 (y-origin 25, slight y-overlap with row 2)
              , Grid<5> { Origin<5> { 0, 25, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {3, 2, 4, 1, 1} }
              , Grid<5> { Origin<5> { 8, 25, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {2, 2, 3, 1, 1} }
              , Grid<5> { Origin<5> {19, 25, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {3, 2, 5, 1, 1} }
              };
  }

  [[nodiscard]] inline auto multiple_survey_10_sixteen() -> Data<3>
  {
    return make_data
                ( make_rectangular_three_d_survey_grids
                  ( std::array<Coordinate, 4> {0, 138, 304, 492}
                  , std::array<Coordinate, 4> {0, 118, 254, 406}
                  , std::array<Coordinate, 4> {62, 58, 64, 60}
                  , std::array<Coordinate, 4> {52, 48, 54, 50}
                  , std::array<Coordinate, 4> {18, 20, 19, 21}
                  , std::array<Coordinate, 4> {2, 3, 2, 3}
                  , std::array<Coordinate, 4> {2, 3, 2, 3}
                  , std::array<Coordinate, 4> {4, 5, 3, 4}
                  )
                );
  }

  [[nodiscard]] inline auto multiple_survey_11_thirtysix() -> Data<3>
  {
    return make_data
                ( make_rectangular_three_d_survey_grids
                  ( std::array<Coordinate, 6> {0, 150, 405, 563, 824, 986}
                  , std::array<Coordinate, 6> {0, 152, 385, 543, 794, 964}
                  , std::array<Coordinate, 6> {80, 86, 82, 88, 84, 90}
                  , std::array<Coordinate, 6> {72, 78, 74, 80, 76, 82}
                  , std::array<Coordinate, 6> {36, 40, 38, 42, 39, 43}
                  , std::array<Coordinate, 6> {2, 3, 2, 3, 2, 3}
                  , std::array<Coordinate, 6> {2, 3, 2, 3, 2, 3}
                  , std::array<Coordinate, 6> {4, 5, 3, 4, 5, 3}
                  )
                );
  }

  [[nodiscard]] inline auto multiple_survey_12_fortynine() -> Data<3>
  {
    return make_data
                ( make_rectangular_three_d_survey_grids
                  ( std::array<Coordinate, 7> {0, 340, 874, 1210, 1740, 2088, 2628}
                  , std::array<Coordinate, 7> {0, 296, 764, 1058, 1520, 1824, 2296}
                  , std::array<Coordinate, 7> {180, 186, 182, 188, 184, 190, 178}
                  , std::array<Coordinate, 7> {158, 164, 160, 166, 162, 168, 156}
                  , std::array<Coordinate, 7> {68, 72, 70, 74, 71, 73, 69}
                  , std::array<Coordinate, 7> {2, 3, 2, 3, 2, 3, 2}
                  , std::array<Coordinate, 7> {2, 3, 2, 3, 2, 3, 2}
                  , std::array<Coordinate, 7> {4, 5, 3, 4, 5, 3, 4}
                  )
                );
  }

}
