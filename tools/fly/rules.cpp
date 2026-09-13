// Browser referee for Ultimate Fly. GPL-3.0-or-later.
// Only fixed, public rosters are exposed: no arbitrary position import or search.
#include "../../src/ultimate/position.h"
#include "../../src/ultimate/nnue.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iostream>
#include <random>
#include <stdexcept>
#ifdef FLY_TEACHER
#include "../../src/ultimate/search.h"
#endif
using namespace Stockfish::Ultimate;
#ifndef FLY_TEACHER
std::optional<int> UltimateNnue::evaluate(const Position&) { return std::nullopt; }
#endif
static Position game;
static std::vector<Position> history;
static std::string output;
static unsigned ticks = 0;
static int points(const Position& p, Color c) { return p.material_points(c); }
static void reset(int preset, unsigned seed) {
    game.clear(); history.clear(); ticks = 0;
    const PieceType armies[3][8] = {
      {PieceType::Rook,PieceType::Knight,PieceType::Bishop,PieceType::Queen,PieceType::King,PieceType::Bishop,PieceType::Knight,PieceType::Rook},
      {PieceType::Turtle,PieceType::Ninja,PieceType::Mage,PieceType::Prince,PieceType::King,PieceType::Berserker,PieceType::Sniper,PieceType::Rook},
      {PieceType::Dragon,PieceType::Fisherman,PieceType::Penguin,PieceType::Queen,PieceType::King,PieceType::Goop,PieceType::Knight,PieceType::Bishop}
    };
    // File reflection keeps the armies identical; seed does not disclose secrets.
    for (int color = 0; color < 2; ++color) for (int f = 0; f < 8; ++f) {
        int x = seed % 2 ? 7-f : f;
        if(game.add_piece(armies[std::clamp(preset,0,2)][f], Color(color), (color ? 72 : 0) + x)<0) throw std::runtime_error("Invalid fly roster");
        if(game.add_piece(preset && f % 3 == 0 ? PieceType::Checker : PieceType::Pawn,
                       Color(color), (color ? 64 : 8) + x)<0) throw std::runtime_error("Invalid fly front rank");
    }
}
static std::vector<double> features(const Move& m) {
    const Color us=game.side_to_move();
    const int id=game.piece_on(m.from);
    const int value=id < 0 ? 0 : game.material_points(id);
    const int before=points(game,us)-points(game,~us);
    Position child=game; Undo undo; child.make_move(m,undo);
    const int after=points(child,us)-points(child,~us);
    // Threat exposure is public, bounded one-action capture geometry, not search.
    double risk=0;
    if(child.side_to_move()!=us && id>=0 && child.piece(id).alive) {
      for(const auto& reply:child.legal_moves())
        if(child.is_capture(reply) && reply.to==child.piece(id).square) risk=std::min(1.0,value/12.0);
    }
    const double advance=(us==Color::White ? int(m.to)/8 : 9-int(m.to)/8)/9.0;
    const double central=1.0-std::abs(int(m.to)%8-3.5)/3.5;
    return {std::tanh((after-before)/8.0),risk,advance,central,
      child.side_to_move()!=us && child.in_check() ? 1.0:0.0,
      id>=0 ? std::min(1.0,child.piece(id).cooldown/5.0):0.0,
      std::min(1.0,value/12.0),game.is_capture(m)?1.0:0.0};
}
static bool exhibition_draw() {
    int repeats=1;for(const auto& p:history)if(p.key()==game.key())++repeats;
    return repeats>=3 || ticks>=400;
}
static const char* state() {
    auto moves=game.legal_moves();
    auto reason=game.terminal_reason();
    const auto key=game.key();
    bool drawn=exhibition_draw();
    std::ostringstream s;
    s << "{\"side\":" << int(game.side_to_move()) << ",\"ply\":" << ticks
      << ",\"key\":\"" << key << "\",\"upn\":\"" << game.upn() << "\",\"check\":" << (game.in_check()?"true":"false")
      << ",\"terminal\":" << (drawn ? 7 : int(reason)) << ",\"winner\":";
    if(!drawn && game.winner()) s << int(*game.winner()); else s << -1;
    s << ",\"pieces\":["; bool first=true;
    for(int i=0;i<game.piece_count();++i) {
      const auto& p=game.piece(i); if(!p.alive || !p.onBoard) continue;
      if(!first)s<<','; first=false;
      s<<"{\"id\":"<<i<<",\"type\":\""<<Position::type_name(p.type)<<"\",\"color\":"<<int(p.color)<<",\"square\":"<<int(p.square)<<",\"cooldown\":"<<int(p.cooldown)<<",\"frozen\":"<<int(p.freezeCount)<<"}";
    }
    s << "],\"moves\":[";
    if(reason==TerminalReason::Ongoing && !drawn) for(size_t i=0;i<moves.size();++i) {
      const auto& m=moves[i]; if(i)s<<',';
      s<<"{\"index\":"<<i<<",\"from\":"<<int(m.from)<<",\"to\":"<<int(m.to)<<",\"kind\":"<<int(m.kind)<<",\"notation\":\""<<game.move_to_string(m)<<"\",\"label\":\""<<game.move_to_display_string(m)<<"\",\"features\":[";
      auto f=features(m); for(size_t j=0;j<f.size();++j) {if(j)s<<',';s<<f[j];}
      s<<"]}";
    }
    s<<"]}"; output=s.str(); return output.c_str();
}
extern "C" {
const char* fly_reset(int preset, int seed) { reset(preset,unsigned(seed));return state(); }
const char* fly_state() { return state(); }
const char* fly_move(int index) {
    const auto moves=game.legal_moves();
    if(index<0 || size_t(index)>=moves.size() || game.game_over() || exhibition_draw())return "{\"error\":\"Illegal action\"}";
    history.push_back(game); Undo undo; game.make_move(moves[index],undo); ++ticks; return state();
}
const char* fly_undo() {
    if(!history.empty()) {game=history.back();history.pop_back();--ticks;}
    return state();
}
}
#ifdef FLY_TEACHER
int main() {
  std::string cmd;
  Search search(8);
  while(std::cin>>cmd) {
    if(cmd=="reset") {int a,b;std::cin>>a>>b;std::cout<<fly_reset(a,b)<<std::endl;}
    else if(cmd=="move") {int m;std::cin>>m;std::cout<<fly_move(m)<<std::endl;}
    else if(cmd=="state")std::cout<<state()<<std::endl;
    else if(cmd=="teach") {
      SearchLimits limits;limits.depth=3;limits.nodes=512;limits.useTablebases=false;
      auto result=search.think(game,limits); auto moves=game.legal_moves();int chosen=-1;
      for(size_t i=0;i<moves.size();++i)if(result.bestMove && moves[i]==*result.bestMove)chosen=int(i);
      std::cout<<"{\"index\":"<<chosen<<",\"nodes\":"<<result.nodes<<",\"score\":"<<result.score<<"}"<<std::endl;
    }
  }
}
#endif
