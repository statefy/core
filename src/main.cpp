#include "finite_automata.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace
{

    void print_section(const std::string &title)
    {
        std::cout << "\n== " << title << " ==\n";
    }

    void print_stateset(const fa::StateSet &s)
    {
        std::cout << "{";
        for (std::size_t i = 0; i < s.v.size(); ++i)
        {
            if (i > 0)
                std::cout << ",";
            std::cout << s.v[i];
        }
        std::cout << "}";
    }

    template <typename T>
    void print_symbol(const T &s)
    {
        std::cout << s;
    }

} // namespace

int main()
{
    using fa::DFA;
    using fa::NFA;
    using fa::StateId;

    // -------------------------
    // DFA<char> example
    // Language: even number of '1's (classic DFA)
    // -------------------------
    print_section("DFA<char> (even number of 1s)");
    DFA<char> d;
    constexpr StateId q0 = 0; // even
    constexpr StateId q1 = 1; // odd

    d.set_start(q0);
    d.set_accepting(q0, true);
    d.set_accepting(q1, false);
    d.set_label(q0, "even");
    d.set_label(q1, "odd");
    d.set_position(q0, fa::Position{100.0f, 100.0f});
    d.set_position(q1, fa::Position{260.0f, 100.0f});

    d.set_transition(q0, '0', q0);
    d.set_transition(q0, '1', q1);
    d.set_transition(q1, '0', q1);
    d.set_transition(q1, '1', q0);

    for (const std::string &s : {"", "1", "11", "10101", "1110"})
    {
        auto r = d.run(s);
        std::cout << "input=\"" << s << "\" => "
                  << (r.accepted ? "ACCEPT" : "REJECT")
                  << " (complete=" << (r.complete ? "yes" : "no")
                  << ", consumed=" << r.consumed << ")\n";
    }

    auto trace = d.run_trace(std::string("101"));
    std::cout << "trace for \"101\":\n";
    for (const auto &step : trace.steps)
    {
        std::cout << "  i=" << step.index
                  << " sym=";
        print_symbol(step.symbol);
        std::cout << " from=" << step.from
                  << " to=" << (step.to ? std::to_string(*step.to) : "dead")
                  << " accepting_after=" << (step.accepting_after ? "yes" : "no")
                  << "\n";
    }

    // Snapshot + JSON round-trip
    auto snap = d.snapshot();
    std::string json;
    auto err = fa::json::serialize(snap, json);
    if (!err.ok)
    {
        std::cout << "serialize failed: " << err.message << "\n";
    }
    else
    {
        std::cout << "serialized snapshot (bytes=" << json.size() << ")\n";
        std::cout << json << "\n";
    }

    fa::GraphSnapshot<char> loaded;
    err = fa::json::deserialize(json, loaded);
    if (!err.ok)
    {
        std::cout << "deserialize failed: " << err.message << "\n";
    }
    else
    {
        std::cout << "deserialized snapshot: nodes=" << loaded.nodes.size()
                  << ", edges=" << loaded.edges.size() << "\n";
    }

    // Editing operations demo
    print_section("DFA edit operations");
    DFA<char> edit = d;
    edit.remove_transition(q0, '1');
    auto r_edit = edit.run(std::string("1"));
    std::cout << "after removing transition (q0,'1'): complete="
              << (r_edit.complete ? "yes" : "no")
              << ", consumed=" << r_edit.consumed << "\n";
    std::cout << "next_state_id() => "
              << (edit.next_state_id() ? std::to_string(*edit.next_state_id()) : "none")
              << "\n";
    edit.clear();
    std::cout << "after clear(): states=" << edit.states().size()
              << ", transitions=" << edit.transition_count() << "\n";

    // -------------------------
    // NFA<char> with epsilon transitions
    // Language: "a" or "b"
    // -------------------------
    print_section("NFA<char> with epsilon");
    NFA<char> n;
    constexpr StateId s0 = 0;
    constexpr StateId s1 = 1;
    constexpr StateId s2 = 2;
    constexpr StateId s3 = 3;

    n.set_start(s0);
    n.set_accepting(s3, true);
    n.set_label(s0, "start");
    n.set_label(s3, "accept");
    n.set_position(s0, fa::Position{100.0f, 250.0f});
    n.set_position(s1, fa::Position{220.0f, 200.0f});
    n.set_position(s2, fa::Position{220.0f, 300.0f});
    n.set_position(s3, fa::Position{360.0f, 250.0f});

    n.add_epsilon(s0, s1);
    n.add_epsilon(s0, s2);
    n.add_transition(s1, 'a', s3);
    n.add_transition(s2, 'b', s3);

    for (const std::string &s : {"a", "b", "ab", ""})
    {
        auto r = n.run(s);
        std::cout << "input=\"" << s << "\" => "
                  << (r.accepted ? "ACCEPT" : "REJECT")
                  << " end_states=";
        print_stateset(r.end_states);
        std::cout << "\n";
    }

    auto ntrace = n.run_trace(std::string("a"));
    std::cout << "trace for \"a\":\n";
    for (const auto &step : ntrace.steps)
    {
        std::cout << "  i=" << step.index << " sym=";
        print_symbol(step.symbol);
        std::cout << " before=";
        print_stateset(step.before);
        std::cout << " after=";
        print_stateset(step.after);
        std::cout << "\n";
    }

    // NFA -> DFA -> minimize
    std::vector<char> alpha = {'a', 'b'};
    auto dfa_from_nfa = n.to_dfa(alpha);
    dfa_from_nfa.make_total(99, alpha);
    auto minimized = dfa_from_nfa.minimize(alpha);
    std::cout << "nfa->dfa states=" << dfa_from_nfa.states().size()
              << " minimized=" << minimized.states().size() << "\n";

    // -------------------------
    // DFA<std::string> token example
    // Language: OPEN followed by any number of CLOSE (including zero CLOSE)
    // -------------------------
    print_section("DFA<std::string> token stream");
    DFA<std::string> token_dfa;
    constexpr StateId t0 = 0;
    constexpr StateId t1 = 1;
    constexpr StateId t_dead = 2;

    token_dfa.set_start(t0);
    token_dfa.set_accepting(t1, true);
    token_dfa.set_transition(t0, "OPEN", t1);
    token_dfa.set_transition(t1, "CLOSE", t1);
    token_dfa.set_transition(t1, "OPEN", t_dead);
    token_dfa.set_transition(t_dead, "OPEN", t_dead);
    token_dfa.set_transition(t_dead, "CLOSE", t_dead);

    std::vector<std::string> tokens1 = {"OPEN"};
    std::vector<std::string> tokens2 = {"OPEN", "CLOSE", "CLOSE"};
    std::vector<std::string> tokens3 = {"CLOSE"};

    for (const auto &tokens : {tokens1, tokens2, tokens3})
    {
        auto r = token_dfa.run(tokens.begin(), tokens.end());
        std::cout << "tokens=[";
        for (std::size_t i = 0; i < tokens.size(); ++i)
        {
            if (i > 0)
                std::cout << ",";
            std::cout << tokens[i];
        }
        std::cout << "] => " << (r.accepted ? "ACCEPT" : "REJECT")
                  << " (complete=" << (r.complete ? "yes" : "no") << ")\n";
    }

    auto token_snap = token_dfa.snapshot();
    std::string token_json;
    err = fa::json::serialize(token_snap, token_json);
    if (err.ok)
    {
        std::cout << "token DFA snapshot bytes=" << token_json.size() << "\n";
    }
    else
    {
        std::cout << "token DFA serialize failed: " << err.message << "\n";
    }

    return 0;
}
