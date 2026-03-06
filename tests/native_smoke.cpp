#include <core/finite_automata.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace
{

    bool expect(bool condition, const std::string &message)
    {
        if (!condition)
        {
            std::cerr << "native smoke test failed: " << message << '\n';
            return false;
        }
        return true;
    }

} // namespace

int main()
{
    bool ok = true;

    fa::DFA<char> dfa;
    dfa.set_start(0);
    dfa.set_accepting(2, true);
    dfa.set_transition(0, 'a', 1);
    dfa.set_transition(1, 'b', 2);

    auto dfa_run = dfa.run(std::string("ab"));
    ok &= expect(dfa_run.accepted, "dfa should accept \"ab\"");
    ok &= expect(dfa_run.complete, "dfa run should be complete");

    auto dfa_trace = dfa.run_trace(std::string("ab"));
    ok &= expect(dfa_trace.end_state.has_value() && *dfa_trace.end_state == 2,
                 "dfa trace should end in state 2");

    auto snapshot = dfa.snapshot();
    std::string json;
    auto err = fa::json::serialize(snapshot, json);
    ok &= expect(err.ok, err.message.empty() ? "dfa serialization failed" : err.message);

    fa::GraphSnapshot<char> round_trip;
    err = fa::json::deserialize(json, round_trip);
    ok &= expect(err.ok, err.message.empty() ? "dfa deserialization failed" : err.message);
    ok &= expect(round_trip.nodes.size() == 3, "dfa round-trip node count mismatch");

    fa::NFA<char> nfa;
    nfa.set_start(0);
    nfa.set_accepting(3, true);
    nfa.add_epsilon(0, 1);
    nfa.add_epsilon(0, 2);
    nfa.add_transition(1, 'a', 3);
    nfa.add_transition(2, 'b', 3);

    auto nfa_a = nfa.run(std::string("a"));
    auto nfa_b = nfa.run(std::string("b"));
    auto nfa_empty = nfa.run(std::string(""));

    ok &= expect(nfa_a.accepted, "nfa should accept \"a\"");
    ok &= expect(nfa_b.accepted, "nfa should accept \"b\"");
    ok &= expect(!nfa_empty.accepted, "nfa should reject empty input");

    auto minimized = nfa.to_dfa(std::vector<char>{'a', 'b'}).minimize(std::vector<char>{'a', 'b'});
    ok &= expect(!minimized.states().empty(), "nfa to dfa minimization should produce states");

    if (!ok)
    {
        return 1;
    }

    std::cout << "native smoke test passed\n";
    return 0;
}
