#ifdef __EMSCRIPTEN__
#include <core/finite_automata.hpp>
#include <emscripten/bind.h>
#include <string>
#include <vector>

namespace wasm_detail
{

    using Symbol = std::string;

    fa::Error parse_symbol_array(std::string_view json, std::vector<Symbol> &out)
    {
        out.clear();
        fa::json::detail::Reader r(json);
        auto err = r.expect('[', "JSON: expected '[' for input array");
        if (!err.ok)
            return err;
        r.skip_ws();
        if (r.consume(']'))
            return fa::Error::success();
        while (true)
        {
            std::string sym;
            err = r.parse_string(sym);
            if (!err.ok)
                return err;
            out.push_back(std::move(sym));
            r.skip_ws();
            if (r.consume(']'))
                break;
            err = r.expect(',', "JSON: expected ',' in input array");
            if (!err.ok)
                return err;
        }
        return fa::Error::success();
    }

    std::string error_json(std::string_view msg)
    {
        std::string out;
        out += "{\"ok\":false,\"error\":";
        fa::json::detail::append_json_string(out, msg);
        out += "}";
        return out;
    }

    void append_state_array(std::string &out, const fa::StateSet &s)
    {
        out += "[";
        for (std::size_t i = 0; i < s.v.size(); ++i)
        {
            if (i > 0)
                out += ",";
            out += std::to_string(s.v[i]);
        }
        out += "]";
    }

    void append_state_array(std::string &out, const std::vector<fa::StateId> &s)
    {
        out += "[";
        for (std::size_t i = 0; i < s.size(); ++i)
        {
            if (i > 0)
                out += ",";
            out += std::to_string(s[i]);
        }
        out += "]";
    }

    fa::Error build_dfa_from_snapshot(const fa::GraphSnapshot<Symbol> &snap, fa::DFA<Symbol> &dfa)
    {
        bool has_start = false;
        for (const auto &n : snap.nodes)
        {
            dfa.add_state(n.id);
            dfa.set_accepting(n.id, n.accepting);
            if (n.label)
                dfa.set_label(n.id, *n.label);
            if (n.position)
                dfa.set_position(n.id, *n.position);
            if (n.start)
            {
                if (has_start)
                    return fa::Error::fail("DFA: multiple start states in snapshot");
                dfa.set_start(n.id);
                has_start = true;
            }
        }
        if (!has_start)
            return fa::Error::fail("DFA: missing start state in snapshot");

        for (const auto &e : snap.edges)
        {
            if (e.epsilon)
                return fa::Error::fail("DFA: epsilon edge is not allowed");
            for (const auto &sym : e.symbols)
            {
                auto existing = dfa.transition(e.from, sym);
                if (existing && *existing != e.to)
                    return fa::Error::fail("DFA: conflicting transitions for same (state, symbol)");
                dfa.set_transition(e.from, sym, e.to);
            }
        }

        return dfa.validate();
    }

    fa::Error build_nfa_from_snapshot(const fa::GraphSnapshot<Symbol> &snap, fa::NFA<Symbol> &nfa)
    {
        bool has_start = false;
        for (const auto &n : snap.nodes)
        {
            nfa.add_state(n.id);
            nfa.set_accepting(n.id, n.accepting);
            if (n.label)
                nfa.set_label(n.id, *n.label);
            if (n.position)
                nfa.set_position(n.id, *n.position);
            if (n.start)
            {
                if (has_start)
                    return fa::Error::fail("NFA: multiple start states in snapshot");
                nfa.set_start(n.id);
                has_start = true;
            }
        }
        if (!has_start)
            return fa::Error::fail("NFA: missing start state in snapshot");

        for (const auto &e : snap.edges)
        {
            if (e.epsilon)
            {
                nfa.add_epsilon(e.from, e.to);
                continue;
            }
            for (const auto &sym : e.symbols)
            {
                nfa.add_transition(e.from, sym, e.to);
            }
        }

        return nfa.validate();
    }

} // namespace wasm_detail

std::string dfaSimulateJson(const std::string &snapshot_json, const std::string &input_json)
{
    using namespace wasm_detail;
    using Symbol = std::string;

    fa::GraphSnapshot<Symbol> snap;
    auto err = fa::json::deserialize(snapshot_json, snap);
    if (!err.ok)
        return error_json(err.message);

    fa::DFA<Symbol> dfa;
    err = build_dfa_from_snapshot(snap, dfa);
    if (!err.ok)
        return error_json(err.message);

    std::vector<Symbol> input;
    err = parse_symbol_array(input_json, input);
    if (!err.ok)
        return error_json(err.message);

    auto trace = dfa.run_trace(input.begin(), input.end());

    std::string out;
    out += "{\"ok\":true,\"type\":\"dfa\",\"accepted\":";
    out += (trace.accepted ? "true" : "false");
    out += ",\"complete\":";
    out += (trace.complete ? "true" : "false");
    out += ",\"consumed\":";
    out += std::to_string(trace.consumed);
    out += ",\"end_state\":";
    if (trace.end_state.has_value())
        out += std::to_string(*trace.end_state);
    else
        out += "null";
    out += ",\"trace\":[";

    for (std::size_t i = 0; i < trace.steps.size(); ++i)
    {
        const auto &step = trace.steps[i];
        if (i > 0)
            out += ",";
        out += "{";
        out += "\"index\":";
        out += std::to_string(step.index);
        out += ",\"symbol\":";
        fa::json::detail::append_json_string(out, step.symbol);
        out += ",\"from\":";
        out += std::to_string(step.from);
        out += ",\"to\":";
        if (step.to.has_value())
            out += std::to_string(*step.to);
        else
            out += "null";
        out += ",\"took\":";
        out += (step.took ? "true" : "false");
        out += ",\"accepting_after\":";
        out += (step.accepting_after ? "true" : "false");
        out += "}";
    }

    out += "]}";
    return out;
}

std::string nfaSimulateJson(const std::string &snapshot_json, const std::string &input_json)
{
    using namespace wasm_detail;
    using Symbol = std::string;

    fa::GraphSnapshot<Symbol> snap;
    auto err = fa::json::deserialize(snapshot_json, snap);
    if (!err.ok)
        return error_json(err.message);

    fa::NFA<Symbol> nfa;
    err = build_nfa_from_snapshot(snap, nfa);
    if (!err.ok)
        return error_json(err.message);

    std::vector<Symbol> input;
    err = parse_symbol_array(input_json, input);
    if (!err.ok)
        return error_json(err.message);

    auto trace = nfa.run_trace(input.begin(), input.end());

    std::string out;
    out += "{\"ok\":true,\"type\":\"nfa\",\"accepted\":";
    out += (trace.accepted ? "true" : "false");
    out += ",\"consumed\":";
    out += std::to_string(trace.consumed);
    out += ",\"end_states\":";
    append_state_array(out, trace.end_states);
    out += ",\"trace\":[";

    for (std::size_t i = 0; i < trace.steps.size(); ++i)
    {
        const auto &step = trace.steps[i];
        if (i > 0)
            out += ",";
        out += "{";
        out += "\"index\":";
        out += std::to_string(step.index);
        out += ",\"symbol\":";
        fa::json::detail::append_json_string(out, step.symbol);
        out += ",\"before\":";
        append_state_array(out, step.before);
        out += ",\"after\":";
        append_state_array(out, step.after);
        out += "}";
    }

    out += "]}";
    return out;
}

EMSCRIPTEN_BINDINGS(fa_wasm)
{
    emscripten::function("dfaSimulateJson", &dfaSimulateJson);
    emscripten::function("nfaSimulateJson", &nfaSimulateJson);
}

#endif
