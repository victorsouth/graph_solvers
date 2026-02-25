#pragma once

namespace graphlib {
;

/// @brief Содержит структуры данных для контроля 
/// посещения блоков и вершин при обходя дерева блоков
template <typename ModelIncidences>
class block_tree_topological_sorter {
	/// @brief Граф, для которого построено дерево блоков
	const basic_graph_t<ModelIncidences>& graph;
	/// @brief Дерево блоков, надстроенное над графом
	const block_tree_t<ModelIncidences>& block_tree;
	/// @brief Вершины -> входящие и выходящие ребра и коннекторы блоков
	const std::unordered_map<size_t, cut_vertex_oriented_incidences_t>& cut_incidences;
	// Индексы входных и выходных коннекторов  блоков
	const std::vector<block_connectors_t>& blocks_connectors;
	/// @brief Количество ожидаемых посещений разделяющих вершин - 
	/// количество непосещенных поставщиков или коннекторов блоков, входящих в вершину
	mutable std::unordered_map<size_t, int> cut_vertex_remaining_visits;
	/// @brief Количество ожидаемых посещений блока - количество непосещенных входных точек сочленения блока
	mutable std::vector<int> block_remaining_visits;

public:
	/// @brief Определяем количество ожидаемых посещений вершин и блоков дерева блоков 
	/// для последующей топологической сортировки
	block_tree_topological_sorter(const basic_graph_t<ModelIncidences>& graph, 
        const block_tree_t<ModelIncidences>& block_tree)
		: graph(graph)
		, block_tree(block_tree)
		, cut_incidences(block_tree.get_origented_cut_vertices(graph))
		, blocks_connectors(block_tree.get_block_connectors())
	{
		block_remaining_visits.resize(block_tree.blocks.size());

		//cut_vertex_remaining_visits.resize(incidences_map.size());
		for (const auto& [cut_vertex, cut_vertex_incidences] : cut_incidences) {
			const cut_vertex_incidences_t& inlets = cut_vertex_incidences.inlets;

			// Подсчет количества поставщиков, входящих в вершину, и выходных коннекторов блоков, 
			// соответствующих текущей вершине на дереве блоков
			cut_vertex_remaining_visits[cut_vertex] =
				(int)(inlets.blocks.size() + inlets.edges1.size());

			// Текущая вершина может быть входным коннектором некоторого блока. 
			// Такая вершина увеличивает количество ожидаемых посещений для такого блока
			const std::vector<size_t>& outlet_blocks = cut_vertex_incidences.outlets.blocks;
			for (size_t block : outlet_blocks) {
				block_remaining_visits[block]++;
			}
		}

	}

protected:

	/// @brief Обход в глубину по вершинам графа с пересчетом количества ожидаемых посещений для вершин и блоков
	/// @param block_sequence Для хранения окончательно посещенных элементов дерева
	void dfs(size_t cut_vertex, std::vector<block_tree_binding_t>* block_sequence) const {
		if (--cut_vertex_remaining_visits.at(cut_vertex) != 0)
			return; // еще не все входы точки сочленения cut_vertex обработаны

		block_sequence->emplace_back(block_tree_binding_type::Vertex, cut_vertex);

		const cut_vertex_oriented_incidences_t& incidences = cut_incidences.at(cut_vertex);

		// Если вершина посещена, можно посетить все потребители, выходящие из нее
		const std::vector<size_t>& outlet_edges1 = incidences.outlets.edges1;
		for (size_t edge1 : outlet_edges1) {
			block_sequence->emplace_back(block_tree_binding_type::Edge1, edge1);
		}

		// Блоки, "выходящие" из текущей вершины (т.е. текущая вершина - вход для таких блоков)
		const std::vector<size_t>& outlet_blocks = incidences.outlets.blocks;
		for (size_t block_index : outlet_blocks) {

			// Уменьшаем число ожидаемых посещений блока
			if (--block_remaining_visits[block_index] != 0)
				continue; // не все входы блока посещены, пока рано отмечать посещенным

			// Все входы блока посещены. Блок считается посещенным.
			block_sequence->emplace_back(block_tree_binding_type::Block, block_index);

			// Переходим в точки сочленения, являющиеся выходами блока
			const std::vector<size_t>& outlet_cut_vertices = blocks_connectors[block_index].outlets;
			for (size_t vertex : outlet_cut_vertices) {
				dfs(vertex, block_sequence);
			}
		}

	};

    /// @brief Верифицирует результат топологической сортировки
    /// Критерий - все элементы графа блоков (мосты и блоки, поставщики) попали в топологическую последовательность
    bool check_blocks_edges1_presence(const std::vector<block_tree_binding_t>& sorted_sequence) const {
        std::vector<bool> block_found(block_tree.blocks.size(), false);
        std::vector<bool> edge1_found(block_tree.edges1.size(), false);

        for (const block_tree_binding_t& graph_element : sorted_sequence) {
            switch (graph_element.binding_type)
            {
            case block_tree_binding_type::Block:
                block_found[graph_element.block] = true;
                break;
            case block_tree_binding_type::Edge1:
                edge1_found[graph_element.edge] = true;
            }
        }

        bool all_blocks_found = std::all_of(block_found.begin(), block_found.end(), 
            [](bool value) {return value;});

		bool all_edges1_found = std::all_of(edge1_found.begin(), edge1_found.end(),
			[](bool value) {return value;});

        return (all_blocks_found && all_edges1_found);
    }



public:

	/// @brief Возвращает топологически отсортированные элементы дерева блоков
    /// Предполагается, что все элементы имеют истинную ориентацию (по расходам)
    /// @param ignore_dangling_vertices_wihout_sources Некоторые висячие узлы в ориентированном графе не имеют поставщиков, можно с них начинать обход. 
    /// Если опция false, то такие узлы учитываются. Для эндогенного расчета это пока не нужно, для чистой теории графов может пригодиться
    /// Пример: -->o<----o---->o<--  (тут в середине висячая вершина без притока)
	/// @return Последовательность объектов при обходе дерева блоков (приток/отбор, точка сочленения, блок)
    /// Точку сочленения block_tree_binding_type::Vertex проходим, 
    /// когда все ее входные ребра и все входящие в нее коннекторы блоков полностью пройдены, 
    /// Выходящие ребра и коннекторы блоков в этот момент еще не пройдены
    /// Блок block_tree_binding_type::Block проходим, когда пройдены все его входные коннекторы
	std::vector<block_tree_binding_t> topological_sort(bool ignore_dangling_vertices_wihout_sources = true) const {
        if (ignore_dangling_vertices_wihout_sources == false) {
            throw std::runtime_error("Please, implement me");
        }

		// Контейнер для топологически отсортированных вершины, блоки, односторонние ребра
		std::vector<block_tree_binding_t> sorted_sequence;

		// Обход начинается с ребер-поставщиков
		for (size_t edge : block_tree.edges1) {
			bool is_source = graph.edges1[edge].has_vertex_to();
			if (is_source) {
				sorted_sequence.emplace_back(
					block_tree_binding_t(block_tree_binding_type::Edge1, edge)
				);
				size_t cut_vertex = graph.edges1[edge].to;
				dfs(cut_vertex, &sorted_sequence);
			}
		}

        if (!check_blocks_edges1_presence(sorted_sequence)) {
            throw std::logic_error("Топологическая сортировка выполнена с ошибкой - не все объекты графа блоков отсортированы");
        }

        return sorted_sequence;
	}
};

/// @brief См. описание в block_tree_topological_sorter::topological_sort
template <typename ModelIncidences>
inline std::vector<block_tree_binding_t> topological_sort(const basic_graph_t<ModelIncidences>& graph, 
	const block_tree_t<ModelIncidences>& block_tree, bool ignore_dangling_vertices_wihout_sources = true)
{
	block_tree_topological_sorter sorter(graph, block_tree);
	return sorter.topological_sort(ignore_dangling_vertices_wihout_sources);
}


template <typename ModelIncidences>
inline std::vector<std::size_t> topological_sort_block(
    const graphlib::basic_graph_t<ModelIncidences>& graph,
    const graphlib::biconnected_component_t& block)
{
    using namespace graphlib;

    basic_graph_t<edge_incidences_t> block_subgraph;
    for (std::size_t edge2 : block.edges2) {
        block_subgraph.edges2.emplace_back(graph.edges2[edge2]);
    }

    std::unordered_set<std::size_t> output_vertices;
    for (std::size_t index = 0; index < block.cut_vertices.size(); ++index) {
        if (block.cut_is_sink[index]) {
            output_vertices.insert(block.cut_vertices[index]);
        }
    }

    std::vector<std::size_t> vertex_order;
    block_subgraph.topological_sort(&vertex_order);

    // Убираем выходные вершины
    std::vector<std::size_t> vertex_order_without_sink_vertices;
    for (std::size_t vertex : vertex_order) {
        if (output_vertices.contains(vertex)) {
            vertex_order_without_sink_vertices.push_back(vertex);
        }
    }
    return vertex_order_without_sink_vertices;
}


}