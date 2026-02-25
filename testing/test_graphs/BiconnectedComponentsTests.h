#pragma once

#include "test_graphs.h"


TEST(Core, DISABLED_Cbs) {
    graphlib::basic_graph_t<graphlib::edge_incidences_t> g;
    g.edges2.emplace_back(0, 1);
    g.edges2.emplace_back(1, 2);
    g.edges2.emplace_back(1, 3);
    g.edges2.emplace_back(1, 4);

    g.edges2.emplace_back(2, 3);
    g.edges2.emplace_back(4, 3);

    g.edges2.emplace_back(2, 5);
    g.edges2.emplace_back(3, 6);
    g.edges2.emplace_back(4, 7);

    //auto cbs = g.cbs(0);

    int dummy = 1;
}

TEST(Core, DISABLED_Cycles) {
    graphlib::basic_graph_t<graphlib::edge_incidences_t> g;
    g.edges2.emplace_back(0, 1);
    g.edges2.emplace_back(1, 2);
    g.edges2.emplace_back(1, 3);
    g.edges2.emplace_back(1, 4);

    g.edges2.emplace_back(2, 3);
    g.edges2.emplace_back(4, 3);

    g.edges2.emplace_back(2, 5);
    g.edges2.emplace_back(3, 6);
    g.edges2.emplace_back(4, 7);

    //auto cycles = g.get_grouped_cycles();

    int dummy = 1;
}

TEST(Utils, DISABLED_GetCycles) {

    //using graphlib::mimo_edge_t;
    //using graphlib::connector_t;
    //using std::vector;

    //vector<mimo_edge_t> bridges{
    //    { 11, connector_t::output(1), connector_t::input(2) },
    //    { 12, connector_t::output(2), connector_t(3) },
    //    { 13, connector_t(3), connector_t::input(4) },
    //    { 14, connector_t(3), connector_t::input(5) },
    //    { 15, connector_t::output(4), connector_t(6) },
    //    { 16, connector_t::output(5), connector_t(6) },
    //    { 17, connector_t(6), connector_t::input(7) },
    //};

    //unordered_map<id_type, wstring> nodes
    //{
    //    { 1, L"source"},
    //    { 2, L"pipe"},
    //    { 4, L"pip"},
    //    { 5, L"pipe"},
    //    { 7, L"sink"}
    //};

    //task_topology solver(bridges, nodes);

    //auto cycles = solver.get_cycles();

}


