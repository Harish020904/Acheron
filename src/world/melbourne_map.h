#pragma once

#include "assets/data/melbourne_map_data.h"
#include <cstdint>
#include <cmath>

// Melbourne world coordinate extents (pixels)
// These match what melbourne_map_data.h was generated with
namespace acheron::world
{
    // Road classification enum — matches values in melbourne_map_data.h
    enum class RoadClass : uint8_t
    {
        MotorwayTrunk = 0,  // Motorway, trunk
        Primary       = 1,  // Primary roads (e.g., Swanston St, Flinders St)
        Secondary     = 2,  // Secondary roads
        Tertiary      = 3,  // Tertiary roads
        Residential   = 4,  // Local streets
        Service       = 5,  // Service roads, alleys
        Footway       = 6,  // Pedestrian/cycling (hidden at low zoom)
    };

    // A single node in the Melbourne road graph (world-space position)
    struct RoadNode
    {
        float x;
        float y;
    };

    // A directed edge connecting two nodes
    struct RoadEdge
    {
        uint32_t from_node;
        uint32_t to_node;
        float    length;        // World-space length in pixels
        RoadClass road_class;
        float    congestion;    // [0..1] — updated by TrafficSystem each frame
    };

    // A point-of-interest / building in Melbourne
    struct PointOfInterest
    {
        float    x;
        float    y;
        uint8_t  category;     // 0=hospital,1=school,2=uni,3=park,4=station,5=restaurant,6=gym,7=office
        char     name[48];
    };

    // POI categories
    enum class POICategory : uint8_t
    {
        Hospital    = 0,
        School      = 1,
        University  = 2,
        Park        = 3,
        Station     = 4,
        Restaurant  = 5,
        Gym         = 6,
        Office      = 7,
    };

    struct MelbourneMap
    {
        static constexpr uint32_t MAX_EDGES = 8192;
        static constexpr uint32_t MAX_POIS  = 512;

        // Mirrored from baked data (so runtime can mutate congestion)
        RoadEdge edges[MAX_EDGES];
        uint32_t edge_count = 0;

        PointOfInterest pois[MAX_POIS];
        uint32_t poi_count = 0;

        // Coordinate helpers
        static float world_width()  { return melbourne_data::MAP_WORLD_WIDTH; }
        static float world_height() { return melbourne_data::MAP_WORLD_HEIGHT; }

        // Convert GPS lat/lon → world pixel coords (matches Python tool logic)
        static void gps_to_world(float lat, float lon, float& out_x, float& out_y)
        {
            out_x = (lon - melbourne_data::MAP_LON_MIN) / (melbourne_data::MAP_LON_MAX - melbourne_data::MAP_LON_MIN) * melbourne_data::MAP_WORLD_WIDTH;
            out_y = (1.0f - (lat - melbourne_data::MAP_LAT_MIN) / (melbourne_data::MAP_LAT_MAX - melbourne_data::MAP_LAT_MIN)) * melbourne_data::MAP_WORLD_HEIGHT;
        }

        // Initialize edges from baked data
        bool initialize()
        {
            // Copy from baked constexpr arrays into mutable runtime arrays
            uint32_t count = melbourne_data::EDGE_COUNT;
            if (count > MAX_EDGES) count = MAX_EDGES;
            edge_count = count;
            for (uint32_t i = 0; i < count; ++i)
            {
                const auto& src = melbourne_data::EDGES[i];
                edges[i].from_node  = src.from;
                edges[i].to_node    = src.to;
                edges[i].length     = src.length;
                edges[i].road_class = static_cast<RoadClass>(src.road_class);
                edges[i].congestion = 0.02f;
            }

            // Seed a few hardcoded Melbourne POIs
            _add_poi(-37.8136f, 144.9631f, POICategory::Station,    "Flinders Street Station");
            _add_poi(-37.8102f, 144.9629f, POICategory::Station,    "Melbourne Central Station");
            _add_poi(-37.8090f, 144.9569f, POICategory::Hospital,   "Royal Melbourne Hospital");
            _add_poi(-37.7996f, 144.9626f, POICategory::University, "University of Melbourne");
            _add_poi(-37.8231f, 144.9694f, POICategory::Hospital,   "Alfred Hospital");
            _add_poi(-37.8136f, 144.9566f, POICategory::Station,    "Southern Cross Station");
            _add_poi(-37.8183f, 144.9671f, POICategory::Park,       "Royal Botanic Gardens");
            _add_poi(-37.8044f, 144.9697f, POICategory::Park,       "Fitzroy Gardens");
            _add_poi(-37.8100f, 144.9680f, POICategory::Office,     "Federation Square");
            _add_poi(-37.8144f, 144.9630f, POICategory::Office,     "Melbourne CBD");
            _add_poi(-37.7947f, 144.9616f, POICategory::University, "RMIT University");
            _add_poi(-37.8100f, 144.9570f, POICategory::School,     "Melbourne Grammar");
            _add_poi(-37.8000f, 144.9580f, POICategory::Restaurant, "Chinatown");
            _add_poi(-37.8050f, 144.9530f, POICategory::Gym,        "Etihad Stadium Area");
            _add_poi(-37.8120f, 144.9700f, POICategory::Park,       "Treasury Gardens");

            return true;
        }

    private:
        void _add_poi(float lat, float lon, POICategory cat, const char* name_str)
        {
            if (poi_count >= MAX_POIS) return;
            PointOfInterest& p = pois[poi_count++];
            gps_to_world(lat, lon, p.x, p.y);
            p.category = static_cast<uint8_t>(cat);
            // Copy name safely
            int i = 0;
            while (name_str[i] && i < 47) { p.name[i] = name_str[i]; ++i; }
            p.name[i] = '\0';
        }
    };

} // namespace acheron::world
