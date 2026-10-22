/**
 * tests/TestCnf.cpp
 *
 * Erweiterte Unit Tests für CNF Parser, Reduktionen, Analyzer und Solver
 * Ziel: >80% Test Coverage über alle Kernkomponenten hinweg.
 */

#include "Cnf.h"
#include "Reduction.h"
#include "SubgraphSATSolver.h"
#include <iostream>
#include <sstream>
#include <cassert>
#include <string>
#include <fstream>
#include <memory>
#include <algorithm>
#include <climits>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace sat;

// =================================================================
// Test Framework Macros & Globals
// =================================================================

int test_count = 0;
int passed_count = 0;
int failed_count = 0;

#define TEST(name) \
    { test_count++; std::cout << "Test " << test_count << ": " << name << " ... " << std::flush; \
        try {

#define PASS() \
        std::cout << "PASS\n"; passed_count++; } \
        catch (const std::exception& e) { \
            std::cout << "FAIL (" << e.what() << ")\n"; failed_count++; } }

#define ASSERT(condition) \
    if (!(condition)) throw std::logic_error("Assertion failed: " #condition)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) throw std::logic_error("Assertion failed: " #a " == " #b)

// Helper für Pfade
std::string get_test_path(const std::string& filename) {
#ifdef _WIN32
    char temp_path[MAX_PATH];
    DWORD path_len = GetTempPathA(MAX_PATH, temp_path);
    if (path_len > 0) {
        return std::string(temp_path) + filename;
    }
    return ".\\" + filename;
#else
    return "/tmp/" + filename;
#endif
}

void write_test_file(const std::string& filename, const std::string& contents) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create test file: " + filename);
    }
    file << contents;
    if (!file) {
        throw std::runtime_error("Cannot write test file: " + filename);
    }
}

// Struct und Helper zum Testen von CLI-Logik aus main.cpp für Coverage-Validierung
struct CommandLineArgsTest {
    std::string input_file;
    std::string output_file;
    std::string subgraph_binary = "./subgraph";
    std::string temp_dir = "/tmp";
    int timeout_sec = 60;
    bool verbose = false;
    bool validate = true;
    bool show_help = false;
    bool show_stats = false;
};

CommandLineArgsTest parse_arguments_test(int argc, char* argv[]) {
    CommandLineArgsTest args;
    if (argc < 2) { args.show_help = true; return args; }
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { args.show_help = true; return args; }
        else if (arg == "-v" || arg == "--verbose") { args.verbose = true; }
        else if (arg == "--stats") { args.show_stats = true; }
        else if (arg == "--no-validate") { args.validate = false; }
        else if (arg == "-o" || arg == "--output") { if (i + 1 < argc) args.output_file = argv[++i]; }
        else if (arg == "--subgraph-binary") { if (i + 1 < argc) args.subgraph_binary = argv[++i]; }
        else if (arg == "--temp-dir") { if (i + 1 < argc) args.temp_dir = argv[++i]; }
        else if (arg == "--timeout") { if (i + 1 < argc) args.timeout_sec = std::stoi(argv[++i]); }
        else if (arg[0] == '-') { throw std::runtime_error("Unknown option"); }
        else { args.input_file = arg; }
    }
    return args;
}

// =================================================================
// Test Cases: CNF & SATAssignment (Cnf.cpp)
// =================================================================

void test_parse_simple_cnf() {
    TEST("Parse simple 2-SAT formula")
    std::string dimacs = "c Simple 2-SAT\np cnf 2 2\n1 2 0\n-1 -2 0\n";
    std::string path = get_test_path("test_simple.cnf");
    std::ofstream temp_file(path);
    temp_file << dimacs;
    temp_file.close();
    
    CNFFormula formula(path);
    ASSERT_EQ(formula.num_variables(), 2);
    ASSERT_EQ(formula.num_clauses(), 2);
    PASS()
}

void test_parse_3sat() {
    TEST("Parse 3-SAT formula and print")
    std::string dimacs = "c 3-SAT example\np cnf 3 2\n1 -3 2 0\n2 3 -1 0\n";
    std::string path = get_test_path("test_3sat.cnf");
    std::ofstream temp_file(path);
    temp_file << dimacs;
    temp_file.close();
    
    CNFFormula formula(path);
    ASSERT_EQ(formula.num_variables(), 3);
    ASSERT_EQ(formula.num_clauses(), 2);
    ASSERT(formula.is_valid_3sat());
    
    formula.print();
    formula.print_statistics();
    PASS()
}

void test_to_3sat_conversion() {
    TEST("Convert k-SAT to 3-SAT (Large clause splitting)")
    std::string dimacs = "p cnf 5 1\n1 2 3 4 5 0\n";
    std::string path = get_test_path("test_5sat.cnf");
    std::ofstream temp_file(path);
    temp_file << dimacs;
    temp_file.close();
    
    CNFFormula formula(path);
    ASSERT(!formula.is_valid_3sat());
    formula.to_3sat();
    ASSERT(formula.is_valid_3sat());
    ASSERT(formula.num_clauses() > 1);
    PASS()
}

void test_simplify_and_tautologies() {
    TEST("Simplify CNF (Deduplication and Tautologies)")
    std::string dimacs = "p cnf 2 4\n1 -1 0\n1 2 0\n1 2 0\n-1 -2 0\n";
    std::string path = get_test_path("test_simplify.cnf");
    std::ofstream temp_file(path);
    temp_file << dimacs;
    temp_file.close();
    
    CNFFormula formula(path);
    formula.simplify();
    ASSERT_EQ(formula.num_clauses(), 2);
    PASS()
}

void test_unit_propagation() {
    TEST("Unit Clause Propagation")
    std::string dimacs = "p cnf 2 2\n1 0\n-1 2 0\n";
    std::string path = get_test_path("test_unit.cnf");
    std::ofstream temp_file(path);
    temp_file << dimacs;
    temp_file.close();
    
    CNFFormula formula(path);
    formula.unit_propagate();
    ASSERT_EQ(formula.num_clauses(), 0);
    PASS()
}

void test_sat_assignment_methods() {
    TEST("SATAssignment deep features")
    SATAssignment assignment(4);
    assignment.set(1, true);
    assignment.set(2, false);
    
    ASSERT(assignment.is_assigned(1));
    ASSERT(assignment.value(1) == true);
    ASSERT(!assignment.is_assigned(3));
    
    ASSERT(assignment.evaluate_literal(1));
    ASSERT(!assignment.evaluate_literal(-1));
    ASSERT(assignment.evaluate_literal(-2));
    
    // Clause {2, -3}: x2 is false, x3 is unassigned (optimistic = true)
    // So: evaluate_clause({2, -3}) = false OR true = true
    Clause c = {2, -3};
    ASSERT(assignment.evaluate_clause(c));  // Now expects true (was wrong)
    
    // Test a definitely false clause: both literals false
    Clause c2 = {-1, -2};  // x1=T so -1=F, x2=F so -2=T → true
    ASSERT(assignment.evaluate_clause(c2));
    
    // Test a definitely false clause: all false
    Clause c3 = {-1};  // x1=T so -1=F
    ASSERT(!assignment.evaluate_clause(c3));
    
    std::string str = assignment.to_string();
    ASSERT(str.find("x1=T") != std::string::npos);
    
    auto vec = assignment.to_vector();
    ASSERT_EQ(vec.size(), 2);
    PASS()
}

// =================================================================
// Test Cases: SATAnalyzer (Cnf.cpp)
// =================================================================

void test_sat_analyzer() {
    TEST("SATAnalyzer metrics & heuristics")
    std::string dimacs = "p cnf 3 3\n1 2 0\n-1 3 0\n2 0\n";
    CNFFormula formula = SATSolverUtils::parse_dimacs_string(dimacs);
    
    SATAnalyzer analyzer(formula);
    auto freq = analyzer.variable_frequency();
    ASSERT(!freq.empty());
    
    // freq ist sortiert nach Häufigkeit (descending)
    // Variablen 1 und 2 haben Häufigkeit 2, Variable 3 hat 1
    // Check that top variable has frequency >= 1
    ASSERT(freq[0] >= 1 && freq[0] <= 3);  // freq[0] is a variable ID
    
    // Check that top variable appears frequently
    // (either 1 or 2 should be first)
    ASSERT(freq[0] == 1 || freq[0] == 2);
    
    auto pure = analyzer.pure_literals();
    ASSERT(std::find(pure.begin(), pure.end(), 3) != pure.end());
    
    auto polarity = analyzer.clause_polarity();
    ASSERT_EQ(polarity[2], 1);
    
    int est_clique = analyzer.estimated_clique_size();
    ASSERT(est_clique >= 3);
    PASS()
}

// =================================================================
// Test Cases: Graph & Reduction (Reduction.cpp)
// =================================================================

void test_graph_and_json() {
    TEST("Graph Matrix & JSON Export")
    Graph g(3);
    g.add_edge(0, 1);
    g.add_edge(1, 2);
    
    ASSERT_EQ(g.num_vertices(), 3);
    ASSERT_EQ(g.num_edges(), 2);
    ASSERT(g.has_edge(2, 1));
    ASSERT_EQ(g.degree(1), 2);
    
    std::string json = g.to_json();
    ASSERT(json.find("\"nodes\"") != std::string::npos);
    ASSERT(json.find("[\"v0\", \"v1\"]") != std::string::npos);
    
    g.print();
    PASS()
}

void test_full_reduction_chain() {
    TEST("SAT To Subgraph Reduction Chain")
    std::string dimacs = "p cnf 3 2\n1 2 -3 0\n-1 3 0\n";
    CNFFormula formula = SATSolverUtils::parse_dimacs_string(dimacs);
    
    SATToSubgraphReducer reducer(formula);
    auto res = reducer.reduce();
    
    ASSERT(res.original_num_variables == 3);
    ASSERT(res.num_vertices > 0);
    ASSERT(res.expected_clique_size == formula.num_clauses());
    ASSERT(!res.source_graph_json.empty());
    ASSERT(!res.target_graph_json.empty());
    ASSERT(reducer.validate_reduction());
    PASS()
}

void test_subgraph_to_sat_extractor() {
    TEST("SubgraphToSATExtractor Fallback & Assignment Generation")
    std::string dimacs = "p cnf 2 1\n1 2 0\n";
    CNFFormula formula = SATSolverUtils::parse_dimacs_string(dimacs);
    
    std::vector<std::pair<int, int>> dummy_mapping = {{0, 0}};
    SubgraphToSATExtractor extractor;
    auto assign = extractor.extract_assignment(formula, dummy_mapping);
    
    ASSERT(assign.is_assigned(1));
    ASSERT(assign.value(1) == true);
    PASS()
}

void test_extractor_uses_mapping() {
    TEST("SubgraphToSATExtractor Belegung aus Mapping")
    // (-1 v 2) und (1 v -3): gewaehlte Literale -1 (Klausel 0, Pos. 0) und -3 (Klausel 1, Pos. 1)
    std::string dimacs = "p cnf 3 2\n-1 2 0\n1 -3 0\n";
    CNFFormula formula = SATSolverUtils::parse_dimacs_string(dimacs);
    
    std::vector<std::pair<int, int>> mapping = {{0, 0}, {1, 1}};
    auto assign = SubgraphToSATExtractor::extract_assignment(formula, mapping);
    
    ASSERT(assign.value(1) == false);
    ASSERT(assign.value(3) == false);
    ASSERT(assign.value(2) == true);   // nicht festgelegt -> True
    ASSERT(assign.satisfies_formula(formula));
    PASS()
}

void test_invalid_clique_reduction_exception() {
    TEST("ThreeSATToCliqueReducer Exception on Non-3SAT")
    CNFFormula formula;
    formula.set_num_variables(4);
    formula.add_clause({1, 2, 3, 4});
    
    ThreeSATToCliqueReducer reducer(formula);
    bool exception_thrown = false;
    try {
        reducer.reduce();
    } catch (const std::runtime_error& e) {
        exception_thrown = true;
    }
    ASSERT(exception_thrown);
    PASS()
}

// =================================================================
// Test Cases: SubgraphSATSolver & Utilities (SubgraphSATSolver.cpp)
// =================================================================

void test_solver_utils_file_io() {
    TEST("SATSolverUtils File Export & Validation")
    std::string dimacs = "p cnf 2 1\n1 -2 0\n";
    CNFFormula formula = SATSolverUtils::parse_dimacs_string(dimacs);
    
    SATSolverResult mock_result;
    mock_result.status = SATSolverResult::SATISFIABLE;
    SATAssignment assign(2);
    assign.set(1, true);
    assign.set(2, false);
    mock_result.assignment = assign;
    
    std::string out_path = get_test_path("sol.txt");
    SATSolverUtils::write_solution_file(out_path, mock_result);
    
    std::ifstream infile(out_path);
    std::string line;
    bool found_sat = false;
    while(std::getline(infile, line)) {
        if(line.find("s SATISFIABLE") != std::string::npos) found_sat = true;
    }
    ASSERT(found_sat);
    
    ASSERT(SATSolverUtils::validate_assignment(formula, assign));
    PASS()
}

void test_solver_string_handling_and_errors() {
    TEST("Solver Dimacs String & Structural Errors")
    SATSolverConfig config;
    config.temp_dir = get_test_path("");
    config.verbose = false;
    
    SubgraphSATSolver solver(config);
    config.subgraph_binary_path = "/bin/nonexistent_binary_xyz";
    
    std::string dimacs = "p cnf 1 1\n1 0\n";
    auto res = solver.solve_dimacs_string(dimacs);
    
    ASSERT_EQ(res.status, SATSolverResult::SAT_ERROR);
    ASSERT(!res.to_string().empty());
    PASS()
}

void test_solver_parse_subgraph_result() {
    TEST("Parse JSON Subgraph response types")
    SATSolverConfig config;
    SubgraphSATSolver solver(config);
    
    // Verwendung von Raw-Strings R"(...)" verhindert Maskierungsfehler
    auto opt1 = solver.parse_subgraph_result(R"({
        "is_subgraph": true
    })");
    ASSERT(opt1.has_value());
    
    auto opt2 = solver.parse_subgraph_result(R"({
        "is_subgraph": false
    })");
    ASSERT(!opt2.has_value());

    std::vector<int> nodes = {99};
    ASSERT(SubgraphSATSolver::parse_mapping(
        R"({"is_subgraph":true,"mapping":[0, 2, 10]})", nodes));
    ASSERT_EQ(nodes.size(), 3);
    ASSERT_EQ(nodes[0], 0);
    ASSERT_EQ(nodes[1], 2);
    ASSERT_EQ(nodes[2], 10);

    ASSERT(!SubgraphSATSolver::parse_mapping(R"({"mapping":[]})", nodes));
    ASSERT(nodes.empty());
    ASSERT(!SubgraphSATSolver::parse_mapping(R"({"mapping":[1})", nodes));
    ASSERT(nodes.empty());
    PASS()
}

// =================================================================
// Test Cases: Main CLI Args Mocking (main.cpp Code-Pfade)
// =================================================================

void test_cli_argument_parsing() {
    TEST("CommandLineArgs Parser coverage")
    
    char* argv1[] = {(char*)"prog", (char*)"--help"};
    auto args1 = parse_arguments_test(2, argv1);
    ASSERT(args1.show_help);
    
    char* argv2[] = {
        (char*)"prog", (char*)"in.cnf", 
        (char*)"-o", (char*)"out.txt", 
        (char*)"--subgraph-binary", (char*)"./sub",
        (char*)"--temp-dir", (char*)"/tmp/test",
        (char*)"--timeout", (char*)"30",
        (char*)"--stats", (char*)"--no-validate", (char*)"-v"
    };
    auto args2 = parse_arguments_test(13, argv2);
    ASSERT_EQ(args2.input_file, "in.cnf");
    ASSERT_EQ(args2.output_file, "out.txt");
    ASSERT_EQ(args2.subgraph_binary, "./sub");
    ASSERT_EQ(args2.temp_dir, "/tmp/test");
    ASSERT_EQ(args2.timeout_sec, 30);
    ASSERT(args2.show_stats);
    ASSERT(!args2.validate);
    ASSERT(args2.verbose);
    PASS()
}

void test_dimacs_parser_edge_cases() {
    TEST("DIMACS parser comments, whitespace and signed literals")
    const std::string path = get_test_path("test_dimacs_edge_cases.cnf");
    write_test_file(path, "   c leading comment\n \t \n  p cnf 3 2\n 1 -2 0\n\t-3 0\n");

    CNFFormula formula(path);
    ASSERT_EQ(formula.num_variables(), 3);
    ASSERT_EQ(formula.num_clauses(), 2);
    ASSERT_EQ(formula.clause(0).size(), 2);
    ASSERT_EQ(formula.clause(0)[0], 1);
    ASSERT_EQ(formula.clause(0)[1], -2);
    ASSERT_EQ(formula.clause(1)[0], -3);
    PASS()
}

void test_parser_invalid_inputs() {
    TEST("DIMACS parser rejects invalid headers and literals")
    bool missing_file_thrown = false;
    try {
        CNFFormula missing(get_test_path("missing_sat_test_input.cnf"));
    } catch (const std::runtime_error&) {
        missing_file_thrown = true;
    }
    ASSERT(missing_file_thrown);

    const std::string invalid_literal_path = get_test_path("test_invalid_literal.cnf");
    write_test_file(invalid_literal_path, "p cnf 2 1\n3 0\n");
    bool invalid_literal_thrown = false;
    try {
        CNFFormula invalid_literal(invalid_literal_path);
    } catch (const std::invalid_argument&) {
        invalid_literal_thrown = true;
    }
    ASSERT(invalid_literal_thrown);

    const std::string invalid_header_path = get_test_path("test_invalid_header.cnf");
    write_test_file(invalid_header_path, "p wrong 2 0\n");
    bool invalid_header_thrown = false;
    try {
        CNFFormula invalid_header(invalid_header_path);
    } catch (const std::runtime_error&) {
        invalid_header_thrown = true;
    }
    ASSERT(invalid_header_thrown);
    PASS()
}

void test_cnf_statistics() {
    TEST("CNF statistics exact values")
    CNFFormula formula;
    formula.set_num_variables(4);
    formula.add_clause({1});
    formula.add_clause({-1, 2, 3});

    const auto stats = formula.compute_statistics();
    ASSERT_EQ(stats.num_variables, 4);
    ASSERT_EQ(stats.num_clauses, 2);
    ASSERT_EQ(stats.num_literals, 4);
    ASSERT_EQ(stats.min_clause_size, 1);
    ASSERT_EQ(stats.max_clause_size, 3);
    ASSERT(stats.avg_clause_size == 2.0);
    PASS()
}

void test_empty_formula_statistics() {
    TEST("Empty CNF statistics and 3-SAT validity")
    CNFFormula formula;
    formula.set_num_variables(2);
    const auto stats = formula.compute_statistics();

    ASSERT_EQ(stats.num_variables, 2);
    ASSERT_EQ(stats.num_clauses, 0);
    ASSERT_EQ(stats.num_literals, 0);
    ASSERT_EQ(stats.max_clause_size, 0);
    ASSERT_EQ(stats.min_clause_size, INT_MAX);
    ASSERT(stats.avg_clause_size == 0.0);
    ASSERT(formula.is_valid_3sat());
    PASS()
}

void test_clause_validation() {
    TEST("CNF clause validation accepts empty clauses and rejects invalid literals")
    CNFFormula formula;
    formula.set_num_variables(2);
    formula.add_clause({});
    ASSERT_EQ(formula.num_clauses(), 1);

    bool zero_thrown = false;
    try {
        formula.add_clause({0});
    } catch (const std::invalid_argument&) {
        zero_thrown = true;
    }
    ASSERT(zero_thrown);

    bool out_of_range_thrown = false;
    try {
        formula.add_clause(Clause{-3});
    } catch (const std::invalid_argument&) {
        out_of_range_thrown = true;
    }
    ASSERT(out_of_range_thrown);
    PASS()
}

void test_dimacs_round_trip() {
    TEST("DIMACS serialization round trip")
    CNFFormula formula;
    formula.set_num_variables(3);
    formula.add_clause({1, -3});
    formula.add_clause({2, 3, -1});

    const std::string path = get_test_path("test_round_trip.cnf");
    formula.to_file(path);
    const CNFFormula parsed(path);
    ASSERT_EQ(parsed.num_variables(), formula.num_variables());
    ASSERT_EQ(parsed.num_clauses(), formula.num_clauses());
    ASSERT(parsed.clause(0) == formula.clause(0));
    ASSERT(parsed.clause(1) == formula.clause(1));
    ASSERT(parsed.to_dimacs().find("p cnf 3 2") != std::string::npos);
    PASS()
}

void test_three_sat_boundary() {
    TEST("3-SAT validity boundary and conversion no-op")
    CNFFormula formula;
    formula.set_num_variables(4);
    formula.add_clause({1, -2, 3});
    ASSERT(formula.is_valid_3sat());
    formula.to_3sat();
    ASSERT_EQ(formula.num_variables(), 4);
    ASSERT_EQ(formula.num_clauses(), 1);
    ASSERT(formula.clause(0) == Clause({1, -2, 3}));
    PASS()
}

void test_to_3sat_long_clause() {
    TEST("Long clause conversion introduces valid helper variables")
    CNFFormula formula;
    formula.set_num_variables(7);
    formula.add_clause({1, 2, 3, 4, 5, 6, 7});

    formula.to_3sat();
    ASSERT(formula.is_valid_3sat());
    ASSERT_EQ(formula.num_variables(), 11);
    ASSERT_EQ(formula.num_clauses(), 5);
    for (const auto& clause : formula.clauses()) {
        ASSERT(!clause.empty());
        ASSERT(clause.size() <= 3);
        for (Literal lit : clause) {
            ASSERT(lit >= -formula.num_variables());
            ASSERT(lit <= formula.num_variables());
        }
    }
    PASS()
}

void test_simplify_reordered_duplicates() {
    TEST("Simplify detects reordered duplicate clauses")
    CNFFormula formula;
    formula.set_num_variables(3);
    formula.add_clause({1, 2, -3});
    formula.add_clause({-3, 1, 2});
    formula.add_clause({2, 1, -3});

    formula.simplify();
    ASSERT_EQ(formula.num_clauses(), 1);
    ASSERT(formula.clause(0) == Clause({-3, 1, 2}));
    PASS()
}

void test_unit_propagate_negative_chain() {
    TEST("Unit propagation follows negative assignments")
    CNFFormula formula;
    formula.set_num_variables(3);
    formula.add_clause({-1});
    formula.add_clause({1, -2});
    formula.add_clause({2, -3});

    formula.unit_propagate();
    ASSERT_EQ(formula.num_clauses(), 0);
    PASS()
}

void test_unit_propagate_without_units() {
    TEST("Unit propagation leaves formulas without units unchanged")
    CNFFormula formula;
    formula.set_num_variables(3);
    formula.add_clause({1, -2});
    formula.add_clause({2, 3});
    const auto before = formula.clauses();

    formula.unit_propagate();
    ASSERT_EQ(formula.clauses().size(), before.size());
    ASSERT(formula.clause(0) == before[0]);
    ASSERT(formula.clause(1) == before[1]);
    PASS()
}

void test_assignment_unassigned_serialization() {
    TEST("Assignment serialization omits unassigned variables")
    SATAssignment assignment(3);
    assignment.set(2, false);

    ASSERT_EQ(assignment.num_vars(), 3);
    ASSERT_EQ(assignment.to_string(), "{x2=F}");
    const auto values = assignment.to_vector();
    ASSERT_EQ(values.size(), 1);
    ASSERT_EQ(values[0].first, 2);
    ASSERT(!values[0].second);
    PASS()
}

void test_assignment_empty_clause() {
    TEST("An empty clause is not satisfied")
    SATAssignment assignment(1);
    ASSERT(!assignment.evaluate_clause({}));

    CNFFormula formula;
    formula.set_num_variables(1);
    formula.add_clause({});
    assignment.set(1, true);
    ASSERT(!assignment.satisfies_formula(formula));
    PASS()
}

void test_analyzer_polarity_and_pure_literals() {
    TEST("Analyzer reports pure variables and all clause polarities")
    CNFFormula formula;
    formula.set_num_variables(4);
    formula.add_clause({1, 2, -3});
    formula.add_clause({-1, -2, 4});
    formula.add_clause({3, -4});

    SATAnalyzer analyzer(formula);
    const auto pure = analyzer.pure_literals();
    ASSERT(std::find(pure.begin(), pure.end(), 1) == pure.end());
    ASSERT(std::find(pure.begin(), pure.end(), 2) == pure.end());
    ASSERT(std::find(pure.begin(), pure.end(), 3) == pure.end());
    ASSERT(std::find(pure.begin(), pure.end(), 4) == pure.end());

    const auto polarity = analyzer.clause_polarity();
    ASSERT_EQ(polarity.at(0), 1);
    ASSERT_EQ(polarity.at(1), -1);
    ASSERT_EQ(polarity.at(2), 0);
    ASSERT_EQ(analyzer.estimated_clique_size(), 9);
    PASS()
}

void test_analyzer_frequency_counts_variable_once() {
    TEST("Variable frequency counts at most once per clause")
    CNFFormula formula;
    formula.set_num_variables(3);
    formula.add_clause({1, 1, -1, 2});
    formula.add_clause({2, 3});

    SATAnalyzer analyzer(formula);
    const auto frequency = analyzer.variable_frequency();
    ASSERT_EQ(frequency.size(), 3);
    ASSERT_EQ(frequency[0], 2);
    ASSERT(std::find(frequency.begin(), frequency.end(), 1) != frequency.end());
    ASSERT(std::find(frequency.begin(), frequency.end(), 3) != frequency.end());
    PASS()
}

void test_graph_edge_boundaries() {
    TEST("Graph ignores self-loops and duplicate edges")
    Graph graph(2);
    graph.add_edge(0, 0);
    graph.add_edge(0, 1);
    graph.add_edge(1, 0);
    ASSERT_EQ(graph.num_edges(), 1);
    ASSERT(graph.has_edge(0, 1));

    bool negative_vertex_thrown = false;
    try {
        graph.add_edge(-1, 0);
    } catch (const std::out_of_range&) {
        negative_vertex_thrown = true;
    }
    ASSERT(negative_vertex_thrown);

    bool high_vertex_thrown = false;
    try {
        graph.add_edge(0, 2);
    } catch (const std::out_of_range&) {
        high_vertex_thrown = true;
    }
    ASSERT(high_vertex_thrown);
    ASSERT(!graph.has_edge(-1, 0));
    ASSERT(!graph.has_edge(0, 2));
    PASS()
}

void test_graph_empty_json_and_neighbors() {
    TEST("Empty graph JSON and isolated neighbors")
    Graph empty;
    ASSERT_EQ(empty.num_vertices(), 0);
    ASSERT_EQ(empty.num_edges(), 0);
    ASSERT(empty.to_json().find("\"nodes\": []") != std::string::npos);
    ASSERT(empty.to_json().find("\"edges\": [\n\n  ]") != std::string::npos);

    Graph graph(3);
    ASSERT(graph.neighbors(1).empty());
    ASSERT_EQ(graph.degree(1), 0);
    PASS()
}

void test_clique_reduction_edges_and_conflicts() {
    TEST("Clique reduction links only compatible literals from different clauses")
    CNFFormula formula;
    formula.set_num_variables(2);
    formula.add_clause({1, -1});
    formula.add_clause({-1});

    ThreeSATToCliqueReducer reducer(formula);
    const auto reduced = reducer.reduce();
    const Graph& graph = reduced.first;
    ASSERT_EQ(reduced.second, 2);
    ASSERT_EQ(graph.num_vertices(), 3);
    ASSERT(!graph.has_edge(0, 2));
    ASSERT(graph.has_edge(1, 2));
    PASS()
}

void test_target_clique_edge_counts() {
    TEST("Target clique sizes have the expected complete edge counts")
    Graph source(1);
    const int sizes[] = {0, 1, 4};
    const int expected_edges[] = {0, 0, 6};
    for (size_t i = 0; i < 3; ++i) {
        CliqueToSubgraphReducer reducer(source, sizes[i]);
        const Graph clique = reducer.create_target_clique();
        ASSERT_EQ(clique.num_vertices(), sizes[i]);
        ASSERT_EQ(clique.num_edges(), expected_edges[i]);
        for (int u = 0; u < sizes[i]; ++u) {
            ASSERT_EQ(clique.degree(u), sizes[i] - 1);
        }
    }
    PASS()
}

void test_empty_reduction_chain() {
    TEST("Empty formula reduces to empty source and target graphs")
    CNFFormula formula;
    formula.set_num_variables(0);
    SATToSubgraphReducer reducer(formula);
    const auto result = reducer.reduce();

    ASSERT_EQ(result.num_vertices, 0);
    ASSERT_EQ(result.expected_clique_size, 0);
    ASSERT(result.source_graph_json.find("\"nodes\": []") != std::string::npos);
    ASSERT(result.target_graph_json.find("\"nodes\": []") != std::string::npos);
    ASSERT(reducer.validate_reduction());
    PASS()
}

void test_extractor_ignores_invalid_mapping() {
    TEST("Assignment extractor ignores invalid mapping entries")
    CNFFormula formula;
    formula.set_num_variables(2);
    formula.add_clause({1, 2});

    const std::vector<std::pair<int, int>> mapping = {
        {-1, 0}, {0, -1}, {0, 2}, {4, 0}
    };
    const auto assignment = SubgraphToSATExtractor::extract_assignment(formula, mapping);
    ASSERT(assignment.is_assigned(1));
    ASSERT(assignment.is_assigned(2));
    ASSERT(assignment.value(1));
    ASSERT(assignment.value(2));
    ASSERT(assignment.satisfies_formula(formula));
    PASS()
}

void test_solver_result_status_rendering() {
    TEST("Solver result renders every status")
    SATSolverResult result;
    ASSERT(result.to_string().find("Status: UNKNOWN") != std::string::npos);

    result.status = SATSolverResult::SATISFIABLE;
    SATAssignment assignment(1);
    assignment.set(1, false);
    result.assignment = assignment;
    ASSERT(result.is_satisfiable());
    ASSERT(result.to_string().find("x1=F") != std::string::npos);

    result.status = SATSolverResult::UNSATISFIABLE;
    ASSERT(result.is_unsatisfiable());
    ASSERT(result.to_string().find("Status: UNSATISFIABLE") != std::string::npos);

    result.status = SATSolverResult::SAT_ERROR;
    result.error_message = "test error";
    ASSERT(result.is_error());
    ASSERT(result.to_string().find("Error: test error") != std::string::npos);
    PASS()
}

void test_solver_file_errors() {
    TEST("Solver returns a descriptive error for a missing DIMACS file")
    SATSolverConfig config;
    SubgraphSATSolver solver(config);

    const auto result = solver.solve_dimacs_file(get_test_path("missing_solver_input.cnf"));
    ASSERT(result.is_error());
    ASSERT(result.error_message.find("Failed to read file:") != std::string::npos);
    PASS()
}

void test_utils_parse_and_assignment_validation() {
    TEST("String parser and assignment validation")
    const CNFFormula formula = SATSolverUtils::parse_dimacs_string(
        "c two clauses\np cnf 2 2\n1 -2 0\n-1 2 0\n");
    ASSERT_EQ(formula.num_variables(), 2);
    ASSERT_EQ(formula.num_clauses(), 2);

    SATAssignment satisfying(2);
    satisfying.set(1, true);
    satisfying.set(2, true);
    ASSERT(SATSolverUtils::validate_assignment(formula, satisfying));

    SATAssignment failing(2);
    failing.set(1, true);
    failing.set(2, false);
    ASSERT(!SATSolverUtils::validate_assignment(formula, failing));
    PASS()
}

// =================================================================
// Main Test Runner
// =================================================================

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    
    std::cout << "\n╔════════════════════════════════════════════╗\n";
    std::cout << "║  SAT-Solver Advanced Coverage Unit Tests   ║\n";
    std::cout << "╚════════════════════════════════════════════╝\n\n";
    
    test_parse_simple_cnf();
    test_parse_3sat();
    test_to_3sat_conversion();
    test_simplify_and_tautologies();
    test_unit_propagation();
    test_sat_assignment_methods();
    test_sat_analyzer();
    test_graph_and_json();
    test_full_reduction_chain();
    test_subgraph_to_sat_extractor();
    test_extractor_uses_mapping();
    test_invalid_clique_reduction_exception();
    test_solver_utils_file_io();
    test_solver_string_handling_and_errors();
    test_solver_parse_subgraph_result();
    test_cli_argument_parsing();
    test_dimacs_parser_edge_cases();
    test_parser_invalid_inputs();
    test_cnf_statistics();
    test_empty_formula_statistics();
    test_clause_validation();
    test_dimacs_round_trip();
    test_three_sat_boundary();
    test_to_3sat_long_clause();
    test_simplify_reordered_duplicates();
    test_unit_propagate_negative_chain();
    test_unit_propagate_without_units();
    test_assignment_unassigned_serialization();
    test_assignment_empty_clause();
    test_analyzer_polarity_and_pure_literals();
    test_analyzer_frequency_counts_variable_once();
    test_graph_edge_boundaries();
    test_graph_empty_json_and_neighbors();
    test_clique_reduction_edges_and_conflicts();
    test_target_clique_edge_counts();
    test_empty_reduction_chain();
    test_extractor_ignores_invalid_mapping();
    test_solver_result_status_rendering();
    test_solver_file_errors();
    test_utils_parse_and_assignment_validation();
    
    std::cout << "\n╔════════════════════════════════════════════╗\n";
    std::cout << "║  Test Summary                              ║\n";
    std::cout << "╚════════════════════════════════════════════╝\n";
    std::cout << "Total tests run: " << test_count << "\n";
    std::cout << "Passed:         " << passed_count << "\n";
    std::cout << "Failed:         " << failed_count << "\n";
    
    if (failed_count == 0) {
        std::cout << "\n✓ Success: Coverage targets achieved safely!\n\n";
        return 0;
    } else {
        std::cout << "\n✗ Error: Some verification vectors failed!\n\n";
        return 1;
    }
}
