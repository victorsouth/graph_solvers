#pragma once

#include <boost/graph/biconnected_components.hpp>
#include <boost/graph/adjacency_list.hpp>

namespace graphlib {
;

/// @brief Блок графа (компонент с вершинной двусвязностью)
/// https://en.wikipedia.org/wiki/Biconnected_component
struct biconnected_component_t {
    /// @brief Точки сочленения - вершины с нумерацией по исходному (!) графу
    std::vector<size_t> cut_vertices;
    /// @brief Ориентация потока в точке сочленения по отношению к блоку 
    /// Если входной, то поток входит в блок, иначе - выходит
    /// (если пустой, то нет ориентации) 
    /// ВАЖНО: односторонние ребра порождают точки сочленения на блоках, но сами в состав блока не входят
    std::vector<bool> cut_is_sink;

    /// @brief Двусторонние ребра блока
    std::vector<size_t> edges2;    

    /// @brief Возвращает локальный индекс точки сочленения для блока по её глобальному индексу
    size_t get_local_cut_vertex_index(size_t cut_vertex) const {
        size_t local_index_of_vertex = std::distance(
            cut_vertices.begin(),
            std::find(cut_vertices.begin(), cut_vertices.end(), cut_vertex));
        return local_index_of_vertex;
    }
};

/// @brief Типы привязок к элементам дерева блоков
enum class block_tree_binding_type {
    Block, // Привязка к блоку (без уточнения точки сочленения)
    Vertex, // Привязка к вершине дерева блоков (НЕ к точке сочленения некоторого блока)
    Block_cutvertex, // Привязка к точке сочленения относительно блока (для привязки расхода и эндогенных свойств)
    Edge1 // Привязка к поставщику/потребителю
};

/// @brief Привязка к элементу дерева блоков
struct block_tree_binding_t {
    /// @brief Тип привязки
    block_tree_binding_type binding_type{ block_tree_binding_type::Block };

    union {
        size_t block; // идентификатор блока на дереве блоков
        size_t vertex; // идентификатор вершины (из исходного графа)
        std::array<size_t, 2> block_cutvertex; // идентификатор блока + локальный идентификатор разделяющей вершины
        size_t edge; // идентификатор одностороннего ребра (из исходного графа)
    };

    /// @brief Оператор сравнения двух привязок на графе блоков
    bool operator==(const block_tree_binding_t& other) const {
        if (binding_type != other.binding_type) {
            return false;
        }
        switch (binding_type) {
        case block_tree_binding_type::Block_cutvertex:
            return block_cutvertex == other.block_cutvertex;
        default:
            return edge == other.edge;
        }
    }


    /// @brief Формирует привязку к Вершине, Блоку, Одностороннему ребру
    block_tree_binding_t(block_tree_binding_type _binding_type, size_t binding) {
        binding_type = _binding_type;
        switch (binding_type) {
        case block_tree_binding_type::Vertex:
            vertex = binding;
            break;
        case block_tree_binding_type::Edge1:
            edge = binding;
            break;
        case block_tree_binding_type::Block:
            block = binding;
            break;
        default:
            throw std::logic_error("Wrong binding_type");
        }
    }

    /// @brief Формирует привязку к Точке сочленения конекретного блока
    block_tree_binding_t(block_tree_binding_type _binding_type, size_t block, size_t local_cutvertex_index) {
        binding_type = _binding_type;
        switch (binding_type) {
        case block_tree_binding_type::Block_cutvertex:
            block_cutvertex = { block, local_cutvertex_index };
            break;
        default:
            throw std::logic_error("Wrong binding_type");
        }
    }

};
/// @brief Для маппингов к элементам графа блоков
struct block_tree_binding_hash {
    /// @brief Возвращает хэш для заданной привязки к графу блоков
    std::size_t operator()(const block_tree_binding_t& key) const {
        std::size_t h = std::hash<int>()(static_cast<int>(key.binding_type));
        switch (key.binding_type) {
        case block_tree_binding_type::Block_cutvertex:
            return h ^ std::hash<size_t>()(key.block_cutvertex[0]) ^
                std::hash<size_t>()(key.block_cutvertex[1]);
        default:
            return h ^ std::hash<size_t>()(key.edge);
        }
    }
};

/// @brief Ветви графа блоков
struct block_tree_branches_t
{
    /// @brief Входящие ветви (с поставщиками)
    std::vector<std::vector<block_tree_binding_t>> inlets;
    /// @brief Выходящие ветви (с потребителями)
    std::vector<std::vector<block_tree_binding_t>> outlets;
};


/// @brief Инциденции разделяющей вершины на графе блоков 
/// @warning Без разделения инциденций на входящие в вершину и выходящие из вершины
struct cut_vertex_incidences_t {
    /// @brief Односторонние ребра, инцидентные точке сочленения
    std::vector<size_t> edges1;
    /// @brief Блоки, инцидентные точке сочленения
    std::vector<size_t> blocks;
};

/// @brief Инциденции разделяющей вершины на графе блоков 
/// @warning Инциденции разделены на ВХОДЯЩИЕ в вершину и ВЫХОДЯЩИЕ из вершины
struct cut_vertex_oriented_incidences_t {
    /// @brief Притоки и блоки, входящие В вершину
    cut_vertex_incidences_t inlets;
    /// @brief Притоки и блоки, выходящие ИЗ вершины
    cut_vertex_incidences_t outlets;
};

/// @brief Коннекторы блока (индексы вершин из исходного графа!)
struct block_connectors_t {
    /// @brief Входы блока
    std::vector<size_t> inlets;
    /// @brief Выходы блока
    std::vector<size_t> outlets;
};

/// @brief Дерево блоков поверх исходного графа
template <typename ModelIncidences>
struct block_tree_t {
    /// @brief Индексы односторонних ребер по всем точкам сочленения дерева блоков
    std::vector<size_t> edges1;
    /// @brief Блоки с вершинной двусвязностью
    std::vector<biconnected_component_t> blocks;
    /// @brief Исходный граф, на котором выделены блоки
    const basic_graph_t<ModelIncidences>* graph;
    /// @brief Сопоставление "Двусторонние ребра -> блоки" (todo: сделать методом - вычисление с мемоизацией)
    std::unordered_map<size_t, size_t> edge2_to_block;
protected:
    /// @brief Отображает точки сочленения блока в список смежных односторонних ребер и блоков
    mutable std::unordered_map<size_t, cut_vertex_incidences_t> _cut_vertices;
    /// @brief Отображение "Точка сочленения -> входящие и выходящие инциденции"
    mutable std::unordered_map<size_t, cut_vertex_oriented_incidences_t> _cut_vertices_oriented;
    /// @brief Входные и выходные вершины блоков (индексы вершин из исходного графа)
    mutable std::vector<block_connectors_t> _block_connectors;

public:
    /// @brief Формирует пустую структуру под дерево блоков на заданном транспортном графе
    block_tree_t(const basic_graph_t<ModelIncidences>& graph) 
        : graph(&graph)
    {};
    /// @brief Конструктор копирования
    block_tree_t(const block_tree_t<ModelIncidences>&) = default;
    /// @brief Конструктор перемещения
    block_tree_t(block_tree_t<ModelIncidences>&&) = default;
    /// @brief Оператор присваивания копированием
    block_tree_t<ModelIncidences>& operator=(const block_tree_t<ModelIncidences>&) = default;
    /// @brief Оператор присваивания перемещением
    block_tree_t<ModelIncidences>& operator=(block_tree_t<ModelIncidences>&&) = default;
protected:
    
    /// @brief Формирует отображение точек сочленения блока в список смежных односторонних ребер и блоков
    void create_cut_vertices_map(const basic_graph_t<ModelIncidences>& graph) const {
        std::unordered_map<size_t, size_t> incident_vertices_for_edges1;

        for (size_t edge1 : edges1) {
            //(к удалению: 16.09.2025 (Южанин)):
            /*std::unordered_set<std::size_t>
                vertices = graph.get_incident_vertices_for_edges<std::vector<size_t>>({ edge1 }, {});
            size_t incident_vertex_for_edge = *(vertices.begin());*/

            size_t incident_vertex_for_edge = graph.edges1[edge1].get_vertex_for_single_sided_edge();
            _cut_vertices[incident_vertex_for_edge].edges1.emplace_back(edge1);
        }

        std::unordered_multimap<size_t, size_t> incident_edges1_for_vertices;
        get_inverted_index(incident_vertices_for_edges1, &incident_edges1_for_vertices);

        for (size_t block_index = 0; block_index < blocks.size(); ++block_index) {
            const biconnected_component_t& block = blocks[block_index];
            for (size_t local_cutvertex_index = 0; local_cutvertex_index < block.cut_vertices.size(); ++local_cutvertex_index) {
                size_t vertex = block.cut_vertices[local_cutvertex_index];
                _cut_vertices[vertex].blocks.emplace_back(block_index);
            }
        }
    }

    /// @brief Формирует отображение точек сочленения во входящие и выходящие односторонние рербра и коннекторы блоков
    void create_oriented_cut_vertices_map(const basic_graph_t<ModelIncidences>& graph) const
    {
        std::unordered_map<size_t, size_t> incident_vertices_for_edges1; // одностороннее ребро -> вершина

        for (size_t edge1 : edges1) {
            //(к удалению: 16.09.2025 (Южанин)) size_t incident_vertex_for_edge = *(graph.get_incident_vertices_for_edges<std::vector<size_t>>({ edge1 }, {}).begin());
            size_t incident_vertex_for_edge = graph.edges1[edge1].get_vertex_for_single_sided_edge();

            //incident_vertices_for_edges1[edge1] = incident_vertex_for_edge;
            bool is_inlet = graph.edges1[edge1].has_vertex_to();
            if (is_inlet) {
                // С точки зрения вершины ребро-поставщик это вход
                _cut_vertices_oriented[incident_vertex_for_edge].inlets.edges1.emplace_back(edge1);
            }
            else {
                _cut_vertices_oriented[incident_vertex_for_edge].outlets.edges1.emplace_back(edge1);
            }
        }
        
        std::unordered_multimap<size_t, size_t> incident_edges1_for_vertices;
        get_inverted_index(incident_vertices_for_edges1, &incident_edges1_for_vertices);

        for (size_t block_index = 0; block_index < blocks.size(); ++block_index) {
            const biconnected_component_t& block = blocks[block_index];

            for (size_t local_cutvertex_index = 0; local_cutvertex_index < block.cut_vertices.size(); ++local_cutvertex_index) {
                size_t vertex = block.cut_vertices[local_cutvertex_index];
                bool is_outlet = block.cut_is_sink[local_cutvertex_index];
                if (is_outlet) {
                    // для блока точка сочленения вход, с точки зрения вершины - это выход
                    _cut_vertices_oriented[vertex].outlets.blocks.emplace_back(block_index);
                }
                else {
                    _cut_vertices_oriented[vertex].inlets.blocks.emplace_back(block_index);
                }

            }
        }
    }

    /// @brief Формирует список входных и выходных точек сочленения для каждого блока
    void create_block_connectors_map() const {
        _block_connectors.resize(blocks.size());
        for (size_t block_id = 0; block_id < blocks.size(); block_id++)  {
            const biconnected_component_t& block = blocks[block_id];
            block_connectors_t connectors;
            for (size_t local_cutvertex_index = 0; local_cutvertex_index < block.cut_vertices.size(); ++local_cutvertex_index)
            {
                size_t cut_vertex = block.cut_vertices[local_cutvertex_index]; // индекс в исходном графе
                if (block.cut_is_sink[local_cutvertex_index]) {
                    connectors.inlets.push_back(cut_vertex);
                }
                else {
                    connectors.outlets.push_back(cut_vertex);
                }
            }
            _block_connectors[block_id] = connectors;
        }
    }

    

public:
    /// @brief Возвращает отображение точек сочленения блока в список смежных односторонних ребер и блоков
    const std::unordered_map<size_t, cut_vertex_incidences_t>& get_cut_vertices(const basic_graph_t<ModelIncidences>& graph) const {
        if (_cut_vertices.empty()) {
            create_cut_vertices_map(graph);
        }
        return _cut_vertices;
    }

    /// @brief Возвращает отображение точек сочленения в входящие и выходящие односторонние ребра и коннекторы блоков
    const std::unordered_map<size_t, cut_vertex_oriented_incidences_t>& get_origented_cut_vertices(const basic_graph_t<ModelIncidences>& graph) const {
        if (_cut_vertices_oriented.empty()) {
            create_oriented_cut_vertices_map(graph);
        }
        return _cut_vertices_oriented;
    }

    /// @brief Возвращает список входных и выходных точек сочленения для каждого блока
    const std::vector<block_connectors_t>& get_block_connectors() const {
        if (_block_connectors.empty()) {
            create_block_connectors_map();
        }
        return _block_connectors;
    }

    /// @brief Возвращает индекс одностороннеого ребра на дереве блоков
    /// (индекс ребра на дереве блоков может не совпадать с индексом ребра в исходном графе)
    /// @param edge Индекс ребра в исходном графе
    /// @return 
    size_t get_local_edge1_index(size_t edge) const {
        size_t local_index_of_edge = std::distance(
            edges1.begin(),
            std::find(edges1.begin(), edges1.end(), edge));
        return local_index_of_edge;
    }

    /// @brief Переориентирует точки сочленения блоков в соответствии со знаком расхода
    /// @param _cut_flows Расходы на точках сочленения
    inline void redirect_cut_vertices_with_flows(std::vector<std::vector<double>>* _cut_flows) {
        auto& cut_flows = *_cut_flows;
        for (size_t block = 0; block < cut_flows.size(); ++block) {
            for (size_t vertex = 0; vertex < cut_flows[block].size(); ++vertex) {
                if (cut_flows[block][vertex] < 0) {
                    this->blocks[block].cut_is_sink[vertex] = !this->blocks[block].cut_is_sink[vertex];
                    cut_flows[block][vertex] *= (-1);
                }
            }
        }

        // Т.к. сменили ориентацию точек сочленения, 
        // обновим списки входов и выходов блоков
        create_block_connectors_map();
    }

};

/// @brief Формирует сопоставление между идентификаторами ДВУСТОРОННИХ ребер и 
/// идентификаторами блоков, в которые они входят 
/// @param blocks Блоки 
/// @return [ID ребра; ID блока]
inline std::unordered_map<size_t, size_t> get_block_edge2_inverted_index(
    const std::vector<biconnected_component_t>& blocks)
{
    std::unordered_map<size_t, size_t> inverted_index; // обратный индекс цикла по вершинам
    for (size_t block_index = 0; block_index < blocks.size(); ++block_index) {
        for (auto cycle_edge2 : blocks[block_index].edges2) {
            inverted_index[cycle_edge2] = block_index;
        }
    }
    return inverted_index;
}

/// @brief Конвертер графа CoreLib в формат Boost Graph Library
template <typename ModelIncidences>
struct block_tree_builder_BGL {

    /// @brief Для сопоставления ребру индекса блока, в который оно входит
    struct edge_component_t
	{
		/// @brief Организация работы с пользовательскими свойствами BGL требует явного определния, что свойство сопоставляется ребрам
		typedef boost::edge_property_tag kind;
		/// @brief Индекс блока
		std::size_t id{ 0 };
	};

	/// @brief Определение в BGL неориентированного графа, ребрам которого сопоставлено свойство типа size_t
	typedef boost::adjacency_list < boost::vecS, boost::vecS, boost::undirectedS,
		boost::no_property, boost::property < edge_component_t, std::size_t > > graph_t;
    /// @brief Для работы с вершинами графа BGL
    typedef boost::graph_traits < graph_t >::vertex_descriptor vertex_t;

	/// @brief Преобразует транспортный граф в граф BGL
	static graph_t convert(const basic_graph_t<ModelIncidences>& tgraph) {
        const vertex_incidences_map_t& vertex_incidences = tgraph.get_vertex_incidences();
        size_t num_vertex = vertex_incidences.size();
		graph_t bgl_graph(num_vertex);

		for (const auto& edge2 : tgraph.edges2) {
			add_edge(edge2.from, edge2.to, bgl_graph);
		}

		return bgl_graph;
	}

	/// @brief Формирует список ребер и точек сочленения блоков на графе BGL
    /// Вершины, инцидентные ребрам edge1, считаются точками сочленения
	/// @return std::vector( std::pair{ребра блока; точки сочленения блока} )
	static std::vector<std::pair<std::vector<size_t>, std::unordered_set<size_t>>> get_edges_cutvertices_by_bicomps(graph_t& bgl_graph, const basic_graph_t<ModelIncidences>& tgraph) {

        typedef typename boost::property_map<graph_t, edge_component_t>::type edge_component_map_t;
        edge_component_map_t component = boost::get(edge_component_t(), bgl_graph);

		// Точки сочленения графа (в графе нет edge1 в явном виде! 
        // Общая вершина двух мостов почему-то не обнаружимвается boost:articulation_points!)
        std::vector<vertex_t> cut_vertices_bgl;
		boost::articulation_points(bgl_graph, std::back_inserter(cut_vertices_bgl));

        // Добавляем как точки сочленения все вершины, инцидентные притокам транспортного графа
        for (const auto& edge1 : tgraph.edges1) {
            const size_t vertex = edge1.get_vertex_for_single_sided_edge();
            cut_vertices_bgl.emplace_back(vertex);
        }

		std::unordered_set<size_t> cut_vertices;
		for (const vertex_t& vertex : cut_vertices_bgl) {
			cut_vertices.emplace(boost::get(boost::vertex_index, bgl_graph, vertex));
		}

        std::size_t num_comps = boost::biconnected_components(bgl_graph, component);
        std::vector<std::pair<std::vector<size_t>, std::unordered_set<size_t>>> result(num_comps);

		size_t edge2_index = 0;

        // Формируем точки сочленения по каждому блоку
        typedef typename boost::graph_traits<graph_t>::edge_iterator edge_iterator_t;
        edge_iterator_t ei, ei_end;
		for (boost::tie(ei, ei_end) = boost::edges(bgl_graph); ei != ei_end; ++ei, ++edge2_index) {


            
            size_t bicomp_index_for_edge = get(component, *ei);//component[*ei];

			size_t start_vertex = source(*ei, bgl_graph);
			size_t end_vertex = target(*ei, bgl_graph);

			if (cut_vertices.count(start_vertex))
				result[bicomp_index_for_edge].second.emplace(start_vertex);

			if (cut_vertices.count(end_vertex))
				result[bicomp_index_for_edge].second.emplace(end_vertex);

			result[bicomp_index_for_edge].first.push_back(edge2_index);
		}
		return result;
	};

    /// @brief добавляет вершины изолированных мостов в список точек сочленения
	/// Концы изолированного моста не считаются точками сочленения. 
    /// Поэтому для мостов явно задаем точки сочленения
    static void update_bicomps_with_bridges_cutvertice(const basic_graph_t<ModelIncidences>& tgraph,
        std::vector<std::pair<std::vector<size_t>, std::unordered_set<size_t>>>& bicomps ) 
    {
		for (size_t bicomp_index = 0; bicomp_index < bicomps.size(); ++bicomp_index) {
			const std::vector<size_t>& bicomp_edges = bicomps[bicomp_index].first;

			if (bicomp_edges.size() != 1)
				continue;

			size_t bridge = bicomp_edges.front();

            size_t bridge_start_vertex = tgraph.edges2[bridge].from;
            size_t bridge_end_vertex = tgraph.edges2[bridge].to;

            bicomps[bicomp_index].second.insert({ bridge_start_vertex, bridge_end_vertex });
		}
    }


public:

	/// @brief Конвертирует граф CoreLib в формат BGL
	/// Графы эквивалентны с точностью до нумерации блоков
	static block_tree_t<ModelIncidences> get_block_tree(const basic_graph_t<ModelIncidences>& tgraph) {

		block_tree_t<ModelIncidences> result(tgraph);

		graph_t bgl_graph = convert(tgraph);

		std::vector<std::pair<std::vector<size_t>, std::unordered_set<size_t>>> bicomps = 
            get_edges_cutvertices_by_bicomps(bgl_graph, tgraph);

        update_bicomps_with_bridges_cutvertice(tgraph, bicomps);

		// Компоненты связности, обнаруженные boost, преобразуем в компоненты связности tlib
        for (const auto& [edges, cut_vertices] : bicomps) {
			biconnected_component_t transoprt_block;
			transoprt_block.edges2 = edges;
			transoprt_block.cut_vertices = std::vector<size_t>(cut_vertices.begin(), cut_vertices.end());
			// все точки сочленения считаем входами
			transoprt_block.cut_is_sink.resize(transoprt_block.cut_vertices.size(), true);
			result.blocks.emplace_back(transoprt_block);
		}

		// Задаем ориентацию коннекторов для мостов
		for (biconnected_component_t& block : result.blocks) {
			if (block.edges2.size() != 1)
				continue;
			size_t vertex_to = tgraph.edges2[block.edges2.front()].to;
            size_t vertex_to_index = block.get_local_cut_vertex_index(vertex_to);
			block.cut_is_sink[vertex_to_index] = false; // выход ребра = выход моста
		}

		result.edge2_to_block = get_block_edge2_inverted_index(result.blocks);

		for (size_t edge1 = 0; edge1 < tgraph.edges1.size(); ++edge1) {
			result.edges1.push_back(edge1);
		}

		return std::move(result);
	};

};

/// @brief Формирует дерево блоков на связном или несвязном графе
/// @tparam ModelIncidences Модели ребер базового графа
/// @warning Заменен на block_tree_builder_BGL - построение дерева блоков по алгоритму BiComp из Boost
template <typename ModelIncidences>
class block_tree_builder_t {

	/// @brief Перенос блоков из block_tree_forest в block_tree с
    /// изменением нумерации ребер в блоках - с нумерации разрезанного графа 
	/// на нумерацию связного графа
    template <typename Graph>
    inline static void transfer_blocks_from_tree_on_forest(const general_structured_graph_t<Graph>& forest_decomposition,
        const block_tree_t<ModelIncidences>& block_tree_forest,
        block_tree_t<ModelIncidences>& target_block_tree)
    {
		for (biconnected_component_t block : block_tree_forest.blocks) {
			size_t forest_subgraph_id = 0; // всегда так из-за логики в prepare_forest
			for (size_t& edge2 : block.edges2) {
				subgraph_binding_t forest_binding(0, graph_binding_type::Edge2, edge2);
				graph_binding_t graph_binding = forest_decomposition.ascend_map.at(forest_binding);
				edge2 = graph_binding.edge;
			}
			for (size_t& vertex : block.cut_vertices) {
				subgraph_binding_t forest_binding(0, graph_binding_type::Vertex, vertex);
				graph_binding_t graph_binding = forest_decomposition.ascend_map.at(forest_binding);
				vertex = graph_binding.vertex; // есть подозрение, что не нужно, т.к. в лесе нумерация вершин стабильна
			}
            target_block_tree.blocks.push_back(block);
		}
    }

	/// @brief Перенос односторонних ребер из block_tree_forest в block_tree
	template <typename Graph>
    inline static void transfer_edges1_from_tree_on_forest(const Graph& graph,
        const general_structured_graph_t<Graph>& forest_decomposition,
        const block_tree_t<ModelIncidences>& block_tree_forest,
        block_tree_t<ModelIncidences>& target_block_tree)
    {
        const Graph& forest = forest_decomposition.subgraphs.begin()->second;
        
        for (size_t edge1 : block_tree_forest.edges1) {
			subgraph_binding_t forest_binding(0, graph_binding_type::Edge1, edge1);
			graph_binding_t graph_binding = forest_decomposition.ascend_map.at(forest_binding);
			if (forest.edges1[edge1].model->is_zero_flow() == false) {
				// НЕ фиктивный поставщик, потребитель. Добавляется в граф блоков
				if (graph_binding.binding_type != graph_binding_type::Edge1)
					throw std::runtime_error("Unexpected ascend binding (expected Edge1)");

                target_block_tree.edges1.push_back(graph_binding.edge);
			}
			else {
				if (graph_binding.binding_type != graph_binding_type::Edge2)
					throw std::runtime_error("Unexpected ascend binding (expected Edge1)");
				if (forest.edges1[edge1].has_vertex_to())
					continue;  // поскольку расход распался на пару поставщик/потребитель, надо только один раз УЗР восстановить

				const auto& model = graph.edges2[graph_binding.edge];

				// В сшитом графе ребро с расходом будет мостом
				biconnected_component_t block;
				block.cut_vertices = { model.from , model.to };
				block.cut_is_sink = { true, false };
				block.edges2 = { graph_binding.edge };

                target_block_tree.blocks.emplace_back(std::move(block));
			}
		}

    }


private:
    static std::unordered_map<size_t, std::unordered_set<size_t>> get_edge2_to_block_inverted_index(
        const std::vector<subgraph_t>& blocks)
    {
        std::unordered_map<size_t, std::unordered_set<size_t>> block_inverted_index; // обратный индекс цикла по вершинам
        for (size_t block_index = 0; block_index < blocks.size(); ++block_index) {
            for (auto cycle_edge2 : blocks[block_index].edges2)
            {
                block_inverted_index[cycle_edge2].insert(block_index);
            }
        }
        return block_inverted_index;
    }


public:

	/// @brief Формирует дерево блоков поверх исходного графа
    /// @param graph Исходные граф
    /// @return Дерево блоков
	inline static block_tree_t<ModelIncidences> get_block_tree(const basic_graph_t<ModelIncidences>& graph)
	{
		std::vector<subgraph_t> block_subgraphs = graph.get_biconnected_components();

        block_tree_t<ModelIncidences> result(graph);

        // Перенос нетривиальных компонентов двусвязности в целевой структуре block_tree_t
		for (const auto& block_subgraph : block_subgraphs) {
			biconnected_component_t block;
			block.edges2 = block_subgraph.edges2;
			block.cut_vertices = block_subgraph.get_boundary_vertices(graph);
			// все точки сочленения считаем входами
			block.cut_is_sink.resize(block.cut_vertices.size(), true);
			result.blocks.emplace_back(std::move(block));
		}

		std::unordered_map<size_t, std::unordered_set<size_t>> edge2_to_cycle =
            get_edge2_to_block_inverted_index(block_subgraphs);

		// Выявление мостов (тривиальных блоков) создание мостов в целевой структуре
		for (size_t edge2 = 0; edge2 < graph.edges2.size(); ++edge2) {
			if (edge2_to_cycle.find(edge2) != edge2_to_cycle.end()) {
				continue; // если больше одного ребра, то НЕ мост. Выходим
			}
			biconnected_component_t block;
			block.cut_vertices = { graph.edges2[edge2].from , graph.edges2[edge2].to };
			block.cut_is_sink = { true, false };
			block.edges2 = { edge2 };

			result.blocks.emplace_back(std::move(block));
		}
		result.edge2_to_block = get_block_edge2_inverted_index(result.blocks);

		for (size_t edge1 = 0; edge1 < graph.edges1.size(); ++edge1) {
			result.edges1.push_back(edge1);
		}

		return result;
	}

	/// @brief На разрезе графа (выполненном по расходомерам) 
    /// строит дерево блоков. Дерево дополняется мостами, соответствующими разрезанным ребрам
	template <typename Graph>
	inline static block_tree_t<ModelIncidences> ascend_block_tree(
		const Graph& graph,
		const general_structured_graph_t<Graph>& forest_decomposition)
	{
		const Graph& forest = forest_decomposition.subgraphs.begin()->second;

		// Пустой граф блоков - в будущем станет графом блоков на "сшитом" графе
		block_tree_t<ModelIncidences> block_tree(graph);

		// Граф блоков на разрезанном графе - индексация из декомпозированного графа (!)
        //block_tree_t<ModelIncidences> block_tree_forest =
        //    block_tree_builder_BGL<ModelIncidences>::get_block_tree(forest);

        block_tree_t<ModelIncidences> block_tree_forest =
            block_tree_builder_t<ModelIncidences>::get_block_tree(forest);


        // Перенос блоков
        transfer_blocks_from_tree_on_forest(forest_decomposition, block_tree_forest, block_tree);
        
        // Перенос односторонних ребер
        transfer_edges1_from_tree_on_forest(graph, forest_decomposition, block_tree_forest, block_tree);

		block_tree.edge2_to_block = get_block_edge2_inverted_index(block_tree.blocks);
		return block_tree;
	}
};

/// @brief Привязки объектов с неизвестным расходом на дереве блоков 
struct unknown_flows_bindings_t
{
public:
    /// @brief Односторонние ребра
    std::vector<size_t> edges1;
    /// @brief Точки сочленения блоков
    /// Каждому блоку сопоставлено не более одной точки сочленения
    std::vector<std::pair<size_t, size_t>> cut_vertices_of_blocks;
    /// @brief Возвращает количество объектов с неизвестным расходом
    size_t size() const {
        return edges1.size() + cut_vertices_of_blocks.size();
    }
};

}