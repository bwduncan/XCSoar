/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009

	M Roberts (original release)
	Robin Birch <robinb@ruffnready.co.uk>
	Samuel Gisiger <samuel.gisiger@triadis.ch>
	Jeff Goodenough <jeff@enborne.f2s.com>
	Alastair Harrison <aharrison@magic.force9.co.uk>
	Scott Penrose <scottp@dd.com.au>
	John Wharington <jwharington@gmail.com>
	Lars H <lars_hn@hotmail.com>
	Rob Dunning <rob@raspberryridgesheepfarm.com>
	Russell King <rmk@arm.linux.org.uk>
	Paolo Ventafridda <coolwind@email.it>
	Tobias Lohner <tobias@lohner-net.de>
	Mirek Jezek <mjezek@ipplc.cz>
	Max Kellermann <max@duempel.org>
	Tobias Bieniek <tobias.bieniek@gmx.de>

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

#include "FlightStatistics.hpp"
#include "TaskClientUI.hpp"
#include "Screen/Fonts.hpp"
#include "Screen/Graphics.hpp"
#include "Screen/Layout.hpp"
#include "Math/FastMath.h"
#include "Math/Earth.hpp"
#include "Math/Constants.h"
#include "NMEA/Info.hpp"
#include "NMEA/Derived.hpp"
#include "Units.hpp"
#include "Terrain/RasterTerrain.hpp"
#include "Wind/WindStore.hpp"
#include "Language.hpp"
#include "Atmosphere.h"
#include "SettingsComputer.hpp"
#include "Navigation/Geometry/GeoVector.hpp"
#include "GlideSolvers/GlidePolar.hpp"
#include "ChartProjection.hpp"
#include "RenderTask.hpp"
#include "RenderTaskPoint.hpp"
#include "RenderObservationZone.hpp"

#include <algorithm>

#include <stdio.h>

using std::min;
using std::max;

void FlightStatistics::Reset() {
  Lock();
  ThermalAverage.Reset();
  Wind_x.Reset();
  Wind_y.Reset();
  Altitude.Reset();
  Altitude_Base.Reset();
  Altitude_Ceiling.Reset();
  Task_Speed.Reset();
  Altitude_Terrain.Reset();
  Unlock();
}

#include "Screen/Chart.hpp"
#include "Task/Visitors/TaskVisitor.hpp"
#include "Task/Visitors/TaskPointVisitor.hpp"

/**
 * Utility class to draw task leg entry lines
 */
class ChartLegHelper:
  public TaskPointConstVisitor
{
public:
  ChartLegHelper(Chart& chart, const double start_time):
    m_chart(chart),
    m_start_time(start_time)
    {
    };

  void Visit(const UnorderedTaskPoint& tp) {};

  void Visit(const StartPoint& tp) {
    if (tp.has_exited())
      draw(tp);
  };
  void Visit(const FinishPoint& tp) {
    if (tp.has_entered())
      draw(tp);
  };
  void Visit(const AATPoint& tp) {
    if (tp.has_entered())
      draw(tp);
  };
  void Visit(const ASTPoint& tp) {
    if (tp.has_entered())
      draw(tp);
  };
private:
  void draw(const OrderedTaskPoint& tp) {
    double x = ((double)tp.get_state_entered().Time - m_start_time) / 3600.0;
    if (x>=0) {
      m_chart.DrawLine(x, m_chart.getYmin(),
                       x, m_chart.getYmax(),
                       Chart::STYLE_REDTHICK);
    }
  }
  Chart& m_chart;
  const double m_start_time;
};

/**
 * Utility class to draw task leg entry lines of ordered task
 */
class ChartTaskHelper:
  public TaskVisitor
{
public:
  ChartTaskHelper(Chart& chart, const double start_time):
    m_leg_visitor(chart, start_time)
    {
    };

  void Visit(const AbortTask& task) {
  };
  void Visit(const OrderedTask& task) {
    task.tp_CAccept(m_leg_visitor);
  };
  void Visit(const GotoTask& task) {
  };
private:
  ChartLegHelper m_leg_visitor;
};


static void DrawLegs(Chart& chart,
                     const TaskClientUI& task,
                     const NMEA_INFO& basic,
                     const DERIVED_INFO& calculated,
                     const bool task_relative)
{
  if (calculated.common_stats.task_started) {

    const double start_time = task_relative
      ? basic.Time - calculated.common_stats.task_time_elapsed
      : basic.flight.TakeOffTime;

    ChartTaskHelper visitor(chart, start_time);

    task.ordered_CAccept(visitor);
  }
}


void
FlightStatistics::RenderBarograph(Canvas &canvas, const RECT rc,
                                  const NMEA_INFO &nmea_info,
                                  const DERIVED_INFO &derived_info,
                                  const TaskClientUI& task) const
{
  Chart chart(canvas, rc);

  if (Altitude.sum_n < 2) {
    chart.DrawNoData();
    return;
  }

  chart.ScaleXFromData(Altitude);
  chart.ScaleYFromData(Altitude);
  chart.ScaleYFromValue(0);
  chart.ScaleXFromValue(Altitude.x_min + 1.0); // in case no data
  chart.ScaleXFromValue(Altitude.x_min);

  DrawLegs(chart, task, nmea_info, derived_info, false);

  Pen hpHorizonGround(Pen::SOLID, IBLSCALE(1), Chart::GROUND_COLOUR);
  Brush hbHorizonGround(Chart::GROUND_COLOUR);

  canvas.select(hpHorizonGround);
  canvas.select(hbHorizonGround);

  chart.DrawFilledLineGraph(Altitude_Terrain, Chart::GROUND_COLOUR);
  canvas.white_pen();
  canvas.white_brush();

  chart.DrawXGrid(0.5, Altitude.x_min, Chart::STYLE_THINDASHPAPER, 0.5, true);
  chart.DrawYGrid(Units::ToSysUnit(1000, Units::AltitudeUnit), 0,
                  Chart::STYLE_THINDASHPAPER, 1000, true);
  chart.DrawLineGraph(Altitude, Chart::STYLE_MEDIUMBLACK);

  chart.DrawTrend(Altitude_Base, Chart::STYLE_BLUETHIN);
  chart.DrawTrend(Altitude_Ceiling, Chart::STYLE_BLUETHIN);

  chart.DrawXLabel(TEXT("t (hr)"));
  chart.DrawYLabel(TEXT("h"));
}

void
FlightStatistics::RenderSpeed(Canvas &canvas, const RECT rc,
                              const NMEA_INFO &nmea_info,
                              const DERIVED_INFO &derived_info,
                              const TaskClientUI& task) const
{
  Chart chart(canvas, rc);

  if ((Task_Speed.sum_n<2)
      || !task.check_ordered_task()) {
    chart.DrawNoData();
    return;
  }

  chart.ScaleXFromData(Task_Speed);
  chart.ScaleYFromData(Task_Speed);
  chart.ScaleYFromValue( 0);
  chart.ScaleXFromValue(Task_Speed.x_min+1.0); // in case no data
  chart.ScaleXFromValue(Task_Speed.x_min);

  DrawLegs(chart, task, nmea_info, derived_info, true);

  chart.DrawXGrid(0.5, Task_Speed.x_min,
                  Chart::STYLE_THINDASHPAPER, 0.5, true);
  chart.DrawYGrid(Units::ToSysUnit(10, Units::TaskSpeedUnit), 0,
                  Chart::STYLE_THINDASHPAPER, 10, true);
  chart.DrawLineGraph(Task_Speed, Chart::STYLE_MEDIUMBLACK);
  chart.DrawTrend(Task_Speed, Chart::STYLE_BLUETHIN);

  chart.DrawXLabel(TEXT("t (hr)"));
  chart.DrawYLabel(TEXT("V"));
}

void
FlightStatistics::RenderClimb(Canvas &canvas, const RECT rc,
    const GlidePolar& glide_polar) const
{
  Chart chart(canvas, rc);

  if (ThermalAverage.sum_n < 1) {
    chart.DrawNoData();
    return;
  }

  double MACCREADY = glide_polar.get_mc();

  chart.ScaleYFromData(ThermalAverage);
  chart.ScaleYFromValue((MACCREADY + 0.5));
  chart.ScaleYFromValue(0);

  chart.ScaleXFromValue(-1);
  chart.ScaleXFromValue(ThermalAverage.sum_n);

  chart.DrawYGrid(Units::ToSysUnit(1.0, Units::VerticalSpeedUnit), 0,
                  Chart::STYLE_THINDASHPAPER, 1.0, true);
  chart.DrawBarChart(ThermalAverage);

  chart.DrawLine(0, MACCREADY, ThermalAverage.sum_n, MACCREADY,
      Chart::STYLE_REDTHICK);

  chart.DrawLabel(TEXT("MC"), max(0.5, (double)ThermalAverage.sum_n - 1.0),
      MACCREADY);

  chart.DrawTrendN(ThermalAverage, Chart::STYLE_BLUETHIN);

  chart.DrawXLabel(TEXT("n"));
  chart.DrawYLabel(TEXT("w"));
}

void
FlightStatistics::RenderGlidePolar(Canvas &canvas, 
                                   const RECT rc,
                                   const DERIVED_INFO &derived, 
                                   const SETTINGS_COMPUTER &settings_computer,
                                   const GlidePolar& glide_polar) const
{
  int i;
  Chart chart(canvas, rc);

  chart.ScaleYFromValue(0);
  chart.ScaleYFromValue(-glide_polar.get_Smax() * fixed(1.1));
  chart.ScaleXFromValue(glide_polar.get_Vmin() * fixed(0.8));
  chart.ScaleXFromValue(glide_polar.get_Vmax() + fixed_two);

  chart.DrawXGrid(Units::ToSysUnit(10.0, Units::SpeedUnit), 0,
                  Chart::STYLE_THINDASHPAPER, 10.0, true);
  chart.DrawYGrid(Units::ToSysUnit(1.0, Units::VerticalSpeedUnit), 0,
                  Chart::STYLE_THINDASHPAPER, 1.0, true);

  double sinkrate0, sinkrate1;
  double v0 = 0, v1;
  bool v0valid = false;
  int i0 = 0;

  for (i = glide_polar.get_Vmin(); i <= (int)glide_polar.get_Vmax(); ++i) {
    sinkrate0 = -glide_polar.SinkRate(fixed(i));
    sinkrate1 = -glide_polar.SinkRate(fixed(i + 1));
    chart.DrawLine(i, sinkrate0, i + 1, sinkrate1, Chart::STYLE_MEDIUMBLACK);

    if (derived.AverageClimbRateN[i] > 0) {
      v1 = derived.AverageClimbRate[i] / derived.AverageClimbRateN[i];

      if (v0valid)
        chart.DrawLine(i0, v0, i, v1, Chart::STYLE_DASHGREEN);

      v0 = v1;
      i0 = i;
      v0valid = true;
    }
  }

  fixed MACCREADY = glide_polar.get_mc();
  fixed sb = -glide_polar.SinkRate(derived.common_stats.V_block);
  fixed ff = (sb - MACCREADY) / derived.common_stats.V_block;

  chart.DrawLine(0, MACCREADY, glide_polar.get_Vmax(),
      MACCREADY + ff * glide_polar.get_Vmax(), Chart::STYLE_REDTHICK);

  chart.DrawXLabel(TEXT("V"));
  chart.DrawYLabel(TEXT("w"));

  TCHAR text[80];
  canvas.background_transparent();

  _stprintf(text, TEXT("Weight %d kg"),
            (int)glide_polar.get_all_up_weight());
  canvas.text(rc.left + IBLSCALE(30), rc.bottom - IBLSCALE(55), text);

  _stprintf(text, TEXT("Wing loading %.1f kg/m2"),
            (double)glide_polar.get_wing_loading());
  canvas.text(rc.left + IBLSCALE(30), rc.bottom - IBLSCALE(40), text);
}


static void
DrawTrace(Chart &chart, 
          const ChartProjection& proj,
          const TracePointVector& trace,
          Chart::Style style) 
{
  POINT last;
  for (TracePointVector::const_iterator it = trace.begin(); it != trace.end(); ++it) {
    POINT sc;
    proj.LonLat2Screen(it->get_location(), sc);

    if (it != trace.begin()) {
      chart.StyleLine(sc, last, style);
    }
    last = sc;
  }
}

void
FlightStatistics::RenderOLC(Canvas &canvas, 
                            const RECT rc,
                            const NMEA_INFO &nmea_info, 
                            const SETTINGS_COMPUTER &settings_computer,
                            const SETTINGS_MAP &settings_map, 
                            const TracePointVector& olc,
                            const TracePointVector& trace) const
{
  // note: braces used here just to delineate separate main steps of
  // this function.  It's useful to ensure things are done in the right
  // order rather than having a monolithic block of code.

  Chart chart(canvas, rc);

  if (trace.size() < 2) {
    chart.DrawNoData();
    return;
  }

  ChartProjection proj(rc, olc, nmea_info.Location);

  DrawTrace(chart, proj, trace, Chart::STYLE_MEDIUMBLACK);
  DrawTrace(chart, proj, olc, Chart::STYLE_REDTHICK);

  { /// @todo draw aircraft on top
//    chart.DrawLabel(TEXT("+"), nmea_info.Location.Longitude,
//        nmea_info.Location.Latitude);
  }
}


void
FlightStatistics::RenderTask(Canvas &canvas, const RECT rc,
                             const NMEA_INFO &nmea_info, 
                             const SETTINGS_COMPUTER &settings_computer,
                             const SETTINGS_MAP &settings_map, 
                             const TaskClientUI& task,
                             const TracePointVector& trace) const
{
  Chart chart(canvas, rc);

  if (!task.check_task()) {
    chart.DrawNoData();
    return;
  }

  ChartProjection proj(rc, task, nmea_info.Location);

  RenderObservationZone ozv(canvas, proj, settings_map);
  RenderTaskPoint tpv(canvas, proj, settings_map,
                      ozv, false, nmea_info.Location);
  ::RenderTask dv(tpv);
  task.CAccept(dv); 
}


void
FlightStatistics::RenderTemperature(Canvas &canvas, const RECT rc) const
{
  Chart chart(canvas, rc);

  int i;
  float hmin = 10000;
  float hmax = -10000;
  float tmin = (float)CuSonde::maxGroundTemperature;
  float tmax = (float)CuSonde::maxGroundTemperature;

  // find range for scaling of graph
  for (i = 0; i < CUSONDE_NUMLEVELS - 1; i++) {
    if (CuSonde::cslevels[i].nmeasurements) {

      hmin = min(hmin, (float)i);
      hmax = max(hmax, (float)i);

      tmin = min(tmin, (float)min(CuSonde::cslevels[i].tempDry,
          min(CuSonde::cslevels[i].airTemp, CuSonde::cslevels[i].dewpoint)));
      tmax = max(tmax, (float)max(CuSonde::cslevels[i].tempDry,
          (double)max(CuSonde::cslevels[i].airTemp, CuSonde::cslevels[i].dewpoint)));
    }
  }

  if (hmin >= hmax) {
    chart.DrawNoData();
    return;
  }

  chart.ScaleYFromValue(hmin);
  chart.ScaleYFromValue(hmax);
  chart.ScaleXFromValue(tmin);
  chart.ScaleXFromValue(tmax);

  bool labelDry = false;
  bool labelAir = false;
  bool labelDew = false;

  int ipos = 0;

  for (i = 0; i < CUSONDE_NUMLEVELS - 1; i++) {
    if (CuSonde::cslevels[i].nmeasurements
        && CuSonde::cslevels[i + 1].nmeasurements) {

      ipos++;

      chart.DrawLine(CuSonde::cslevels[i].tempDry, i,
          CuSonde::cslevels[i + 1].tempDry, (i + 1), Chart::STYLE_REDTHICK);

      chart.DrawLine(CuSonde::cslevels[i].airTemp, i,
          CuSonde::cslevels[i + 1].airTemp, (i + 1), Chart::STYLE_MEDIUMBLACK);

      chart.DrawLine(CuSonde::cslevels[i].dewpoint, i,
          CuSonde::cslevels[i + 1].dewpoint, i + 1, Chart::STYLE_BLUETHIN);

      if (ipos > 2) {
        if (!labelDry) {
          chart.DrawLabel(TEXT("DALR"), CuSonde::cslevels[i + 1].tempDry, i);
          labelDry = true;
        } else if (!labelAir) {
          chart.DrawLabel(TEXT("Air"), CuSonde::cslevels[i + 1].airTemp, i);
          labelAir = true;
        } else if (!labelDew) {
          chart.DrawLabel(TEXT("Dew"), CuSonde::cslevels[i + 1].dewpoint, i);
          labelDew = true;
        }
      }
    }
  }

  chart.DrawXLabel(TEXT("T")TEXT(DEG));
  chart.DrawYLabel(TEXT("h"));
}

void
FlightStatistics::RenderWind(Canvas &canvas, const RECT rc,
    const NMEA_INFO &nmea_info, const WindStore &wind_store) const
{
  int numsteps = 10;
  int i;
  fixed h;
  Vector wind;
  bool found = true;
  double mag;

  LeastSquares windstats_mag;
  Chart chart(canvas, rc);

  if (Altitude_Ceiling.y_max - Altitude_Ceiling.y_min <= 10) {
    chart.DrawNoData();
    return;
  }

  for (i = 0; i < numsteps; i++) {
    h = fixed(Altitude_Ceiling.y_max - Altitude_Base.y_min) *
      i / fixed(numsteps - 1) + fixed(Altitude_Base.y_min);

    wind = wind_store.GetWind(nmea_info.Time, h, &found);
    mag = hypot(wind.x, wind.y);

    windstats_mag.LeastSquaresUpdate(mag, h);
  }

  chart.ScaleXFromData(windstats_mag);
  chart.ScaleXFromValue(0);
  chart.ScaleXFromValue(10.0);

  chart.ScaleYFromData(windstats_mag);

  chart.DrawXGrid(Units::ToSysUnit(5, Units::SpeedUnit), 0,
                  Chart::STYLE_THINDASHPAPER, 5.0, true);
  chart.DrawYGrid(Units::ToSysUnit(1000, Units::AltitudeUnit), 0,
                  Chart::STYLE_THINDASHPAPER, 1000.0, true);
  chart.DrawLineGraph(windstats_mag, Chart::STYLE_MEDIUMBLACK);

#define WINDVECTORMAG 25

  numsteps = (int)((rc.bottom - rc.top) / WINDVECTORMAG) - 1;

  // draw direction vectors

  double hfact;
  for (i = 0; i < numsteps; i++) {
    hfact = (i + 1) / (double)(numsteps + 1);
    h = (Altitude_Ceiling.y_max - Altitude_Base.y_min) * hfact
        + Altitude_Base.y_min;

    wind = wind_store.GetWind(nmea_info.Time, h, &found);
    if (windstats_mag.x_max == 0)
      windstats_mag.x_max = 1; // prevent /0 problems
    wind.x /= fixed(windstats_mag.x_max);
    wind.y /= fixed(windstats_mag.x_max);
    mag = hypot(wind.x, wind.y);
    if (mag <= 0.0)
      continue;

    Angle angle = Angle::radians(atan2(wind.x, -wind.y));

    chart.DrawArrow((chart.getXmin() + chart.getXmax()) / 2, h,
        mag * WINDVECTORMAG, angle, Chart::STYLE_MEDIUMBLACK);
  }

  chart.DrawXLabel(TEXT("w"));
  chart.DrawYLabel(TEXT("h"));
}

#include "Airspace/AirspaceIntersectionVisitor.hpp"
#include "AirspaceClientUI.hpp"
#include "Airspace/AirspaceCircle.hpp"
#include "Airspace/AirspacePolygon.hpp"


class AirspaceIntersectionVisitorSlice: public AirspaceIntersectionVisitor
{
public:
  AirspaceIntersectionVisitorSlice(Canvas &canvas, Chart &chart,
      const SETTINGS_MAP &settings, const GEOPOINT start) :
    m_canvas(canvas), m_chart(chart), m_settings(settings), m_start(start)
  {
    Pen mpen(Pen::BLANK, 0, Color(0xf0, 0xf0, 0xb0));
    m_canvas.select(mpen);
  }

  void
  Render(const AbstractAirspace& as, int type)
  {
    if (m_intersections.empty()) {
      return;
    }

    m_canvas.select(MapGfx.GetAirspaceBrushByClass(type, m_settings));
    m_canvas.set_text_color(MapGfx.GetAirspaceColourByClass(type, m_settings));

    RECT rcd;
    rcd.top = m_chart.screenY(as.get_top_altitude());
    if (as.is_base_terrain()) {
      rcd.bottom = m_chart.screenY(0);
    } else {
      rcd.bottom = m_chart.screenY(as.get_base_altitude());
    }
    
    for (AirspaceIntersectionVector::const_iterator it =
        m_intersections.begin(); it != m_intersections.end(); ++it) {
      const GEOPOINT p_start = (it->first);
      const GEOPOINT p_end = (it->second);
      const fixed distance_start = m_start.distance(p_start);
      const fixed distance_end = m_start.distance(p_end);

      rcd.left = m_chart.screenX(distance_start);
      rcd.right = m_chart.screenX(distance_end);
      m_canvas.rectangle(rcd.left, rcd.top, rcd.right, rcd.bottom);
    }
  }

  void
  render(const AbstractAirspace& as)
  {
    int type = as.get_type();
    if (type >= 0) {
      Render(as, type);
    }
  }

  void
  Visit(const AirspaceCircle& as)
  {
    render(as);
  }

  void
  Visit(const AirspacePolygon& as)
  {
    render(as);
  }

private:
  Canvas& m_canvas;
  Chart& m_chart;
  const SETTINGS_MAP& m_settings;
  const GEOPOINT& m_start;
};

void
FlightStatistics::RenderAirspace(Canvas &canvas, 
                                 const RECT rc,
                                 const NMEA_INFO &nmea_info, 
                                 const DERIVED_INFO &derived,
                                 const SETTINGS_MAP &settings_map, 
                                 const AirspaceClientUI &airspace_database,
                                 RasterTerrain &terrain) const
{
  static const fixed range(50000); // 50 km
  fixed hmin = max(fixed_zero, nmea_info.GPSAltitude - fixed(3300));
  fixed hmax = max(fixed(3300), nmea_info.GPSAltitude + fixed(1000));
  const GEOPOINT p_start = nmea_info.Location;
  const GeoVector vec(range, nmea_info.TrackBearing);
  const GEOPOINT p_end = vec.end_point(p_start);

  Chart chart(canvas, rc);
  chart.ResetScale();
  chart.ScaleXFromValue(0);
  chart.ScaleXFromValue(range);
  chart.ScaleYFromValue(hmin);
  chart.ScaleYFromValue(hmax);

  // draw airspaces
  AirspaceIntersectionVisitorSlice ivisitor(canvas, chart, settings_map, p_start);
  airspace_database.visit_intersecting(p_start, vec, ivisitor);

  terrain.Lock();

  // draw terrain
  if (terrain.isTerrainLoaded()) {
    std::vector<POINT> points;
    POINT pf0, pf1;
    pf0.x = chart.screenX(range);
    pf0.y = chart.screenY(0);
    points.push_back(pf0);
    pf1.x = chart.screenX(0);
    pf1.y = chart.screenY(0);
    points.push_back(pf1);

    for (unsigned j = 0; j < AIRSPACE_SCANSIZE_X; ++j) {
      const fixed t_this = fixed(j) / (AIRSPACE_SCANSIZE_X - 1);
      const GEOPOINT p_this = p_start + (p_end - p_start) * t_this;

      POINT p;
      p.x = chart.screenX(t_this * range);
      p.y = chart.screenY(terrain.GetTerrainHeight(p_this));

      points.push_back(p);
    }
    points.push_back(points[0]);

    Pen a_pen (Pen::SOLID, IBLSCALE(1), Chart::GROUND_COLOUR);
    Brush a_brush (Chart::GROUND_COLOUR);

    canvas.select(a_pen);
    canvas.select(a_brush);
    canvas.polygon(&points[0], points.size());
  }
  terrain.Unlock();

  // draw aircraft trend line
  if (nmea_info.GroundSpeed > fixed(10)) {
    fixed t = range / nmea_info.GroundSpeed;
    chart.DrawLine(0, nmea_info.GPSAltitude, range,
        nmea_info.GPSAltitude + derived.Average30s * t, Chart::STYLE_BLUETHIN);
  }

  // draw aircraft
  {
    int delta;
    canvas.white_pen();
    canvas.white_brush();

    POINT line[4];
    line[0].x = chart.screenX(0.0);
    line[0].y = chart.screenY(nmea_info.GPSAltitude);
    line[1].x = rc.left;
    line[1].y = line[0].y;
    delta = (line[0].x - line[1].x);
    line[2].x = line[1].x;
    line[2].y = line[0].y - delta / 2;
    line[3].x = (line[1].x + line[0].x) / 2;
    line[3].y = line[0].y;
    canvas.polygon(line, 4);
  }

  // draw grid
  {
    canvas.white_pen();
    canvas.white_brush();
    canvas.set_text_color(Color(0xff, 0xff, 0xff));

    chart.DrawXGrid(Units::ToSysUnit(5.0, Units::DistanceUnit), 0,
                    Chart::STYLE_THINDASHPAPER, 5.0, true);
    chart.DrawYGrid(Units::ToSysUnit(1000.0, Units::AltitudeUnit), 0,
                    Chart::STYLE_THINDASHPAPER, 1000.0, true);

    chart.DrawXLabel(TEXT("D"));
    chart.DrawYLabel(TEXT("h"));
  }
}

void
FlightStatistics::StartTask()
{
  Lock();
  // JMW clear thermal climb average on task start
  ThermalAverage.Reset();
  Task_Speed.Reset();
  Unlock();
}

void
FlightStatistics::AddAltitudeTerrain(const double tflight,
    const double terrainalt)
{
  Lock();
  Altitude_Terrain.LeastSquaresUpdate(max(0.0, tflight / 3600.0), terrainalt);
  Unlock();
}

void
FlightStatistics::AddAltitude(const double tflight, const double alt)
{
  Lock();
  Altitude.LeastSquaresUpdate(max(0.0, tflight / 3600.0), alt);
  Unlock();
}

double
FlightStatistics::AverageThermalAdjusted(const double mc_current,
    const bool circling)
{
  double mc_stats;

  Lock();
  if (ThermalAverage.y_ave > 0) {
    if ((mc_current > 0) && circling) {
      mc_stats = (ThermalAverage.sum_n * ThermalAverage.y_ave + mc_current)
          / (ThermalAverage.sum_n + 1);
    } else {
      mc_stats = ThermalAverage.y_ave;
    }
  } else {
    mc_stats = mc_current;
  }
  Unlock();

  return mc_stats;
}

void
FlightStatistics::AddTaskSpeed(const double tflight, const double val)
{
  Lock();
  Task_Speed.LeastSquaresUpdate(tflight/3600.0, val);
  Unlock();
}

void
FlightStatistics::AddClimbBase(const double tflight, const double alt)
{
  Lock();

  if (Altitude_Ceiling.sum_n > 0) {
    // only update base if have already climbed, otherwise
    // we will catch the takeoff height as the base.
    Altitude_Base.LeastSquaresUpdate(max(0.0, tflight) / 3600.0, alt);
  }

  Unlock();
}

void
FlightStatistics::AddClimbCeiling(const double tflight, const double alt)
{
  Lock();
  Altitude_Ceiling.LeastSquaresUpdate(max(0.0, tflight) / 3600.0, alt);
  Unlock();
}

/**
 * Adds a thermal to the ThermalAverage calculator
 * @param v Average climb speed of the last thermal
 */
void
FlightStatistics::AddThermalAverage(const double v)
{
  Lock();
  ThermalAverage.LeastSquaresUpdate(v);
  Unlock();
}

void
FlightStatistics::CaptionBarograph(TCHAR *sTmp)
{
  Lock();
  if (Altitude_Ceiling.sum_n<2) {
    _stprintf(sTmp, TEXT("\0"));
  } else if (Altitude_Ceiling.sum_n<4) {
    _stprintf(sTmp, TEXT("%s:\r\n  %.0f-%.0f %s"),
              gettext(TEXT("Working band")),
              Units::ToUserUnit(Altitude_Base.y_ave, Units::AltitudeUnit),
              Units::ToUserUnit(Altitude_Ceiling.y_ave, Units::AltitudeUnit),
              Units::GetAltitudeName());
  } else {
    _stprintf(sTmp, TEXT("%s:\r\n  %.0f-%.0f %s\r\n\r\n%s:\r\n  %.0f %s/hr"),
              gettext(TEXT("Working band")),
              Units::ToUserUnit(Altitude_Base.y_ave, Units::AltitudeUnit),
              Units::ToUserUnit(Altitude_Ceiling.y_ave, Units::AltitudeUnit),
              Units::GetAltitudeName(),
              gettext(TEXT("Ceiling trend")),
              Units::ToUserUnit(Altitude_Ceiling.m, Units::AltitudeUnit),
              Units::GetAltitudeName());
  }
  Unlock();
}

void
FlightStatistics::CaptionClimb(TCHAR* sTmp)
{
  Lock();
  if (ThermalAverage.sum_n==0) {
    _stprintf(sTmp, TEXT("\0"));
  } else if (ThermalAverage.sum_n==1) {
    _stprintf(sTmp, TEXT("%s:\r\n  %3.1f %s"),
	      gettext(TEXT("Av climb")),
	      Units::ToUserUnit(ThermalAverage.y_ave, Units::VerticalSpeedUnit),
	      Units::GetVerticalSpeedName()
	      );
  } else {
    _stprintf(sTmp, TEXT("%s:\r\n  %3.1f %s\r\n\r\n%s:\r\n  %3.2f %s"),
	      gettext(TEXT("Av climb")),
	      Units::ToUserUnit(ThermalAverage.y_ave, Units::VerticalSpeedUnit),
	      Units::GetVerticalSpeedName(),
	      gettext(TEXT("Climb trend")),
	      Units::ToUserUnit(ThermalAverage.m, Units::VerticalSpeedUnit),
	      Units::GetVerticalSpeedName()
	      );
  }
  Unlock();
}

void
FlightStatistics::CaptionPolar(TCHAR *sTmp, const GlidePolar& glide_polar) const
{
  if (Layout::landscape) {
    _stprintf(sTmp, TEXT("%s:\r\n  %d\r\n  at %d %s\r\n\r\n%s:\r\n%3.2f %s\r\n  at %d %s"),
	      gettext(TEXT("Best LD")),
	      (int)glide_polar.get_bestLD(),
        (int)(Units::ToUserUnit(glide_polar.get_VbestLD(),
                                Units::SpeedUnit)),
	      Units::GetSpeedName(),
	      gettext(TEXT("Min sink")),
              (double)Units::ToUserUnit(glide_polar.get_Smin(), Units::VerticalSpeedUnit),
	      Units::GetVerticalSpeedName(),
        (int)(Units::ToUserUnit(glide_polar.get_Vmin(),
                                Units::SpeedUnit)),
	      Units::GetSpeedName()
	      );
  } else {
    _stprintf(sTmp, TEXT("%s:\r\n  %d at %d %s\r\n%s:\r\n  %3.2f %s at %d %s"),
	      gettext(TEXT("Best LD")),
        (int)glide_polar.get_bestLD(),
        (int)(Units::ToUserUnit(glide_polar.get_VbestLD(),
                                Units::SpeedUnit)),
	      Units::GetSpeedName(),
	      gettext(TEXT("Min sink")),
              (double)Units::ToUserUnit(glide_polar.get_Smin(), Units::VerticalSpeedUnit),
	      Units::GetVerticalSpeedName(),
        (int)(Units::ToUserUnit(glide_polar.get_Vmin(),
                                Units::SpeedUnit)),
	      Units::GetSpeedName());
  }
}

void
FlightStatistics::CaptionTempTrace(TCHAR *sTmp) const
{
  _stprintf(sTmp, TEXT("%s:\r\n  %5.0f %s\r\n\r\n%s:\r\n  %5.0f %s\r\n"),
	    gettext(TEXT("Thermal height")),
	    Units::ToUserUnit(CuSonde::thermalHeight, Units::AltitudeUnit),
	    Units::GetAltitudeName(),
	    gettext(TEXT("Cloud base")),
	    Units::ToUserUnit(CuSonde::cloudBase, Units::AltitudeUnit),
	    Units::GetAltitudeName());
}

void
FlightStatistics::CaptionTask(TCHAR *sTmp, const DERIVED_INFO &derived) const
{
  const CommonStats &common = derived.common_stats;
  double d_remaining  = derived.task_stats.total.remaining.get_distance();

  if (!common.ordered_valid) {
    _tcscpy(sTmp, gettext(_T("No task")));
  } else {
    TCHAR timetext1[100];
    TCHAR timetext2[100];
    if (common.ordered_has_targets) {
      Units::TimeToText(timetext1, (int)common.task_time_remaining);
      Units::TimeToText(timetext2, (int)common.aat_time_remaining);

      if (Layout::landscape) {
	_stprintf(sTmp,
		  TEXT("%s:\r\n  %s\r\n%s:\r\n  %s\r\n%s:\r\n  %5.0f %s\r\n%s:\r\n  %5.0f %s\r\n"),
		  gettext(TEXT("Task to go")),
		  timetext1,
                    gettext(TEXT("AAT to go")),
		  timetext2,
		  gettext(TEXT("Distance to go")),
                  (double)Units::ToUserUnit(d_remaining, Units::DistanceUnit),
		  Units::GetDistanceName(),
		  gettext(TEXT("Target speed")),
                  (double)Units::ToUserUnit(common.aat_speed_remaining, Units::TaskSpeedUnit),
		  Units::GetTaskSpeedName()
		  );
      } else {
	_stprintf(sTmp,
		  TEXT("%s: %s\r\n%s: %s\r\n%s: %5.0f %s\r\n%s: %5.0f %s\r\n"),
		  gettext(TEXT("Task to go")),
		  timetext1,
		  gettext(TEXT("AAT to go")),
		  timetext2,
		  gettext(TEXT("Distance to go")),
                  (double)Units::ToUserUnit(d_remaining, Units::DistanceUnit),
		  Units::GetDistanceName(),
		  gettext(TEXT("Target speed")),
                  (double)Units::ToUserUnit(common.aat_speed_remaining, Units::TaskSpeedUnit),
		  Units::GetTaskSpeedName()
		  );
      }
    } else {
      Units::TimeToText(timetext1, (int)common.task_time_remaining);
      _stprintf(sTmp, TEXT("%s: %s\r\n%s: %5.0f %s\r\n"),
		gettext(TEXT("Task to go")),
		timetext1,
		gettext(TEXT("Distance to go")),
		(double)Units::ToUserUnit(d_remaining, Units::DistanceUnit),
		Units::GetDistanceName());
    }
  }
}
