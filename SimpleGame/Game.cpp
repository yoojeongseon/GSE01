#include "stdafx.h"
#include "Game.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace {
constexpr int ChunkSize = 8;
// Generation version 1. Keep this function stable for existing prototype saves.
std::uint64_t Hash(std::int64_t x, std::int64_t y) {
    std::uint64_t v = static_cast<std::uint64_t>(x)*0x9E3779B185EBCA87ULL;
    v ^= static_cast<std::uint64_t>(y)*0xC2B2AE3D27D4EB4FULL;
    v ^= 20260908ULL; v ^= v >> 30; v *= 0xBF58476D1CE4E5B9ULL;
    v ^= v >> 27; v *= 0x94D049BB133111EBULL; return v ^ (v >> 31);
}
TileKey Tile(WorldPoint p) {
    return {static_cast<std::int64_t>(std::floor(p.x)),static_cast<std::int64_t>(std::floor(p.y))};
}
TileKey ChunkAt(WorldPoint p) {
    return Tile({p.x/ChunkSize,p.y/ChunkSize});
}
double Distance(WorldPoint a, WorldPoint b) { return std::hypot(a.x-b.x,a.y-b.y); }
bool Road(std::int64_t x, std::int64_t y) {
    // Connected roads leave the initial settlement in four directions.
    return x == 0 || y == 0;
}
Color Tint(Color c, float f) { return {c.r*f,c.g*f,c.b*f,c.a}; }
const Color Ink(.055f,.073f,.087f), Gold(.85f,.65f,.35f), Paper(.82f,.84f,.77f);
void Diamond(Renderer& r, Point p, float w, float h, Color c) {
    r.Quad({p.x,p.y-h},{p.x+w,p.y},{p.x,p.y+h},{p.x-w,p.y},c);
}
void Box(Renderer& r, Point p, float w, float d, float h, Color c) {
    r.Quad({p.x-w,p.y-d},{p.x,p.y},{p.x,p.y-h},{p.x-w,p.y-d-h},Tint(c,.72f));
    r.Quad({p.x,p.y},{p.x+w,p.y-d},{p.x+w,p.y-d-h},{p.x,p.y-h},Tint(c,.9f));
    Diamond(r,{p.x,p.y-d-h},w,d,c);
}
bool ValidPoint(WorldPoint p) {
    // Reject corrupted saves before converting coordinates to integer tile keys.
    return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x)<1.e12 && std::abs(p.y)<1.e12;
}
}

Game::Game() {
    village_ = {
        {{-3,-1.5},Kind::House,0}, {{1.5,-3},Kind::House,1},
        {{4,-.5},Kind::House,2}, {{-4,3},Kind::Ruin,0}, {{5,4},Kind::Ruin,1},
        {{0,0},Kind::Fire,0}, {{-.8,1.2},Kind::Villager,0},
        {{-2,4},Kind::Shrine,0}
    };
    wchar_t executable[32768] = {};
    DWORD count = GetModuleFileNameW(nullptr,executable,32768);
    if (count && count < 32768) {
        std::wstring path(executable,count);
        path = path.substr(0,path.find_last_of(L"\\/")+1) + L"Saves";
        if (!CreateDirectoryW(path.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS) {
            saveBlocked_=true;
            Message("저장 폴더에 쓸 수 없습니다. 진행 상황을 저장할 수 없습니다.");
        }
        savePath_=path+L"\\prototype_v1.txt";
    } else { saveBlocked_=true; Message("저장 경로를 찾을 수 없습니다. 진행 상황을 저장할 수 없습니다."); }
    if (!saveBlocked_ && !Load()) {
        saveBlocked_=true;
        Message("저장 파일을 읽지 못했습니다. 기존 파일은 보존했습니다.\n프로토타입 안내서의 저장 복구 항목을 확인해 주세요.");
    }
    camera_=player_;
    Stream();
}

void Game::Stream() {
    TileKey center=ChunkAt(player_);
    // Fixed 7 x 7 working set, independent of the number of visited regions.
    for (auto it=chunks_.begin(); it!=chunks_.end();) {
        if (std::abs(it->first.first-center.first)>3 || std::abs(it->first.second-center.second)>3)
            it=chunks_.erase(it);
        else ++it;
    }
    for (int dy=-3;dy<=3;++dy) for (int dx=-3;dx<=3;++dx) {
        TileKey key={center.first+dx,center.second+dy};
        if (chunks_.count(key)) continue;
        Chunk chunk;
        for (int y=0;y<ChunkSize;++y) for (int x=0;x<ChunkSize;++x) {
            std::int64_t tx=key.first*ChunkSize+x, ty=key.second*ChunkSize+y;
            if ((std::abs(tx)<7 && std::abs(ty)<7) || Road(tx,ty)) continue;
            auto hash=Hash(tx,ty);
            WorldPoint p={tx+.5,ty+.5};
            if (hash%157==0) chunk.objects.push_back({p,Kind::Shrine,hash});
            else if (hash%17<3) chunk.objects.push_back({p,Kind::Tree,hash});
            else if (hash%31==0) chunk.objects.push_back({p,Kind::Rock,hash});
        }
        chunks_.emplace(key,std::move(chunk));
    }
}
bool Game::Blocked(WorldPoint p) const {
    auto collides=[&](const Object& o) {
        double radius=0;
        switch(o.kind) {
        case Kind::Tree: radius=.36; break; case Kind::Rock: radius=.36; break;
        case Kind::House: radius=1.12; break; case Kind::Ruin: radius=.6; break;
        case Kind::Shrine: radius=.38; break; case Kind::Fire: radius=.34; break;
        default: return false;
        }
        return Distance(p,o.p)<radius+.18;
    };
    for (const auto& o:village_) if(collides(o)) return true;
    TileKey key=ChunkAt(p);
    for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx) {
        auto it=chunks_.find({key.first+dx,key.second+dy});
        if (it!=chunks_.end()) for(const auto& o:it->second.objects) if(collides(o)) return true;
    }
    return false;
}
void Game::Update(float dt, bool up, bool down, bool left, bool right, bool run) {
    time_+=dt; autosave_+=dt; messageTime_=std::max(0.f,messageTime_-dt);
    if (!deathPrompt_) {
        double sx=double(right)-double(left), sy=double(down)-double(up);
        double length=std::hypot(sx,sy);
        if (length>0) {
            sx/=length; sy/=length;
            // Inverse isometric basis: WASD follows screen directions.
            double dx=(sx+sy)*.70710678118, dy=(sy-sx)*.70710678118;
            double step=dt*(run?4.8:2.8);
            WorldPoint old=player_;
            WorldPoint next={player_.x+dx*step,player_.y};
            if (!Blocked(next)) player_=next;
            next={player_.x,player_.y+dy*step};
            if (!Blocked(next)) player_=next;
            double travelled=Distance(old,player_);
            distance_+=travelled; walk_+=static_cast<float>(travelled)*5;
        }
    }
    // Limit camera lag so the loaded region always covers the visible canvas.
    float follow=1.f-std::exp(-9.f*dt);
    camera_.x+=(player_.x-camera_.x)*follow;
    camera_.y+=(player_.y-camera_.y)*follow;
    Stream();
    if (autosave_>=10) { autosave_=0; Save(); }
}
Point Game::Project(WorldPoint p) const {
    double x=p.x-camera_.x, y=p.y-camera_.y;
    return {640.f+static_cast<float>((x-y)*42),390.f+static_cast<float>((x+y)*21)};
}
void Game::Ground(Renderer& r) {
    r.Rect(0,0,1280,800,{.10f,.15f,.16f});
    for (const auto& pair:chunks_) {
        auto cx=pair.first.first*ChunkSize, cy=pair.first.second*ChunkSize;
        for(int y=0;y<ChunkSize;++y) for(int x=0;x<ChunkSize;++x) {
            auto tx=cx+x,ty=cy+y;
            Point p=Project({tx+.5,ty+.5});
            if(p.x<-50 || p.x>1330 || p.y<-30 || p.y>830) continue;
            auto hash=Hash(tx,ty);
            float variation=float(hash%12)*.003f;
            bool clearing=std::hypot(double(tx),double(ty))<5;
            Color ground=clearing?Color(.23f+variation,.245f+variation,.21f+variation):
                                  Color(.13f+variation,.20f+variation,.18f+variation);
            if(Road(tx,ty)) ground={.28f+variation,.275f+variation,.24f+variation};
            Diamond(r,p,42.2f,21.2f,ground);
            if(Road(tx,ty) || clearing) {
                for(int stone=0;stone<3;++stone) {
                    float ox=float((hash>>(stone*8))%35)-17;
                    float oy=float((hash>>(stone*8+4))%13)-6;
                    r.Ellipse(p.x+ox,p.y+oy,3,1.3f,Tint(ground,.8f));
                }
            } else if(hash%3==0) {
                r.Line({p.x-4,p.y+2},{p.x-6,p.y-4},1,{.25f,.32f,.24f});
                r.Line({p.x,p.y+2},{p.x+3,p.y-3},1,{.20f,.29f,.23f});
            }
        }
        if(debug_) {
            Point a=Project({double(cx),double(cy)}),b=Project({double(cx+8),double(cy)});
            Point c=Project({double(cx+8),double(cy+8)}),d=Project({double(cx),double(cy+8)});
            r.Line(a,b,1,{.45f,.7f,.65f,.5f}); r.Line(b,c,1,{.45f,.7f,.65f,.5f});
            r.Line(c,d,1,{.45f,.7f,.65f,.5f}); r.Line(d,a,1,{.45f,.7f,.65f,.5f});
        }
    }
    Point fire=Project({0,0});
    for(int i=7;i>0;--i) r.Ellipse(fire.x,fire.y,28.f+i*15,14.f+i*7,{.92f,.47f,.15f,.025f});
}

void Game::DrawObject(Renderer& r,const Object& o) {
    Point p=Project(o.p);
    if(p.x<-180 || p.x>1460 || p.y<-50 || p.y>1080) return;
    float x=p.x,y=p.y;
    float fade=1.f;
    if(o.kind==Kind::Tree || o.kind==Kind::House) {
        Point hero=Project(player_);
        if(std::abs(hero.x-x)<65 && hero.y<y && hero.y>y-150) fade=.42f;
    }
    r.Ellipse(x+7,y+3,o.kind==Kind::House?68.f:20.f,o.kind==Kind::House?25.f:8.f,{.025f,.04f,.05f,.3f});
    switch(o.kind) {
    case Kind::Tree: {
        float height=72.f+float(o.variation%35);
        r.Rect(x-4,y-33,8,34,{.18f,.16f,.13f,fade});
        for(int i=0;i<3;++i) {
            float top=y-height-i*17,bottom=y-15-i*25,half=36.f-i*6;
            r.Triangle({x,top},{x-half,bottom},{x+half,bottom},{.07f+i*.013f,.15f+i*.016f,.145f+i*.013f,fade});
            r.Triangle({x,top},{x,bottom},{x+half,bottom},{.12f+i*.014f,.22f+i*.018f,.195f+i*.015f,fade});
        }
        break;
    }
    case Kind::Rock:
        Box(r,{x,y},16,8,12,{.38f,.42f,.41f});
        break;
    case Kind::House: {
        Box(r,{x,y},59,28,65,{.39f,.37f,.30f,fade});
        r.Quad({x-69,y-90},{x,y-128},{x,y-67},{x-69,y-35},{.20f,.23f,.24f,fade});
        r.Quad({x,y-128},{x+69,y-90},{x+69,y-35},{x,y-67},{.29f,.31f,.30f,fade});
        for(int i=1;i<5;++i) {
            float t=i/5.f;
            r.Line({x,y-128+61*t},{x+69,y-90+55*t},2,{.16f,.19f,.20f,fade});
        }
        r.Rect(x-30,y-39,16,29,{.105f,.115f,.115f,fade});
        r.Rect(x+20,y-50,15,20,{.94f,.62f,.27f,fade});
        r.Line({x+27,y-50},{x+27,y-30},2,Ink);
        r.Line({x+20,y-40},{x+35,y-40},2,Ink);
        Box(r,{x+34,y-104},9,5,31,{.32f,.34f,.32f,fade});
        for(int i=0;i<4;++i) {
            float t=std::fmod(time_*.3f+i*.25f,1.f);
            r.Ellipse(x+34+t*15,y-145-t*55,7+t*12,5+t*8,{.52f,.56f,.54f,(1-t)*.12f});
        }
        break;
    }
    case Kind::Ruin:
        Box(r,{x-17,y},16,9,65,{.34f,.40f,.40f});
        Box(r,{x+27,y-5},13,8,48,{.36f,.41f,.40f});
        Box(r,{x-4,y-60},29,8,15,{.43f,.47f,.44f});
        r.Line({x-21,y-48},{x-13,y-36},2,Ink);
        r.Line({x-13,y-36},{x-19,y-24},2,Ink);
        Box(r,{x+18,y+8},10,6,7,{.29f,.34f,.32f});
        break;
    case Kind::Shrine: {
        bool found=discoveries_.count(Tile(o.p))!=0;
        for(int i=4;i>0;--i) r.Ellipse(x,y-15,i*12.f,i*8.f,{.38f,.72f,.70f,.025f});
        Box(r,{x,y},18,10,8,{.33f,.40f,.39f});
        Box(r,{x,y-8},8,5,35,{.44f,.52f,.49f});
        Diamond(r,{x,y-36},4,7,found?Gold:Color(.49f,.86f,.83f));
        if(found) {
            for(int i=0;i<5;++i) {
                r.Rect(x-18+i*8,y+3+(i%2)*4,2,5,{.35f,.43f,.28f});
                r.Ellipse(x-17+i*8,y+3+(i%2)*4,3,2,{.84f,.72f,.48f});
            }
        }
        if(Distance(player_,o.p)<1.7) r.Text(x-33,y-72,found?"기억된 장소":"E  살펴보기",Paper,1.5f);
        break;
    }
    case Kind::Fire: {
        for(int i=0;i<8;++i) {
            float a=i*6.2831853f/8;
            r.Ellipse(x+std::cos(a)*19,y+std::sin(a)*9,6,4,{.37f,.38f,.33f});
        }
        r.Line({x-13,y-2},{x+11,y+4},5,{.22f,.14f,.09f});
        r.Line({x+12,y-2},{x-9,y+4},5,{.28f,.17f,.10f});
        float flicker=std::sin(time_*8)*3;
        r.Triangle({x-12,y},{x-4,y-31-flicker},{x+10,y},{.90f,.35f,.10f});
        r.Triangle({x-5,y},{x+4,y-22+flicker},{x+9,y},{1.f,.69f,.22f});
        r.Triangle({x-4,y},{x,y-14},{x+5,y},{1.f,.91f,.58f});
        for(int i=0;i<6;++i) {
            float t=std::fmod(time_*.4f+i*.17f,1.f);
            r.Rect(x+std::sin(i*4.f+t*6)*9,y-12-t*53,2,2,{1.f,.7f,.3f,1-t});
        }
        break;
    }
    case Kind::Villager: case Kind::Heir: case Kind::Player: {
        bool hero=o.kind==Kind::Player, keeper=o.kind==Kind::Villager;
        Color cloak=hero?Color(.40f,.55f,.55f):keeper?Color(.56f,.37f,.23f):Color(.44f,.43f,.60f);
        if(o.kind==Kind::Heir && o.variation==1) cloak={.54f,.52f,.32f};
        if(o.kind==Kind::Heir && o.variation==2) cloak={.32f,.51f,.47f};
        float step=hero?std::sin(walk_)*2:std::sin(time_*1.7f)*.5f;
        r.Rect(x-7,y-9,5,10+step,Ink); r.Rect(x+2,y-9,5,10-step,Ink);
        r.Triangle({x,y-41},{x-14,y-6},{x+14,y-6},Tint(cloak,.65f));
        r.Quad({x-8,y-33},{x+8,y-33},{x+10,y-8},{x-9,y-8},cloak);
        r.Ellipse(x,y-37,9,10,Tint(cloak,.8f));
        r.Rect(x-4,y-38,8,8,{.71f,.61f,.47f});
        r.Rect(x-4,y-39,8,3,Ink);
        r.Line({x+13,y-25},{x+17,y+1},2,{.48f,.37f,.23f});
        if(hero) {
            r.Rect(x+11,y-21,6,8,Gold);
            r.Ellipse(x+14,y-17,16,18,{1.f,.68f,.31f,.045f});
            Diamond(r,{x,y-59},4,3,Gold);
        }
        if(Distance(player_,o.p)<1.8 && !hero)
            r.Text(x-27,y-61,keeper?"E  불지기":"E  이어진 삶",Paper,1.5f);
        break;
    }
    }
}

void Game::Draw(Renderer& r) {
    r.Begin();
    Ground(r);
    std::vector<Object> objects=village_;
    for(const auto& c:chunks_) for(const auto& o:c.second.objects) objects.push_back(o);
    for(const auto& h:heirs_) {
        if(Distance(h.p,player_)<32)
            objects.push_back({h.p,Kind::Heir,h.kindness>0?1ULL:h.distance>20?2ULL:0ULL});
    }
    objects.push_back({player_,Kind::Player,0});
    std::stable_sort(objects.begin(),objects.end(),[](const Object& a,const Object& b) {
        return a.p.x+a.p.y < b.p.x+b.p.y;
    });
    for(const auto& o:objects) DrawObject(r,o);
    // Soft edge darkness and drifting ground mist, before the interface.
    for(int i=0;i<12;++i) {
        float edge=14.f*i;
        r.Rect(edge,0,14,800,{.02f,.04f,.05f,.12f*(1-i/12.f)});
        r.Rect(1266-edge,0,14,800,{.02f,.04f,.05f,.12f*(1-i/12.f)});
        r.Rect(0,i*9.f,1280,9,{.03f,.05f,.06f,.14f*(1-i/12.f)});
    }
    for(int i=0;i<5;++i) {
        float x=std::fmod(time_*5+i*307.f,1700.f)-200;
        r.Ellipse(x,540+i*31.f,210,17,{.52f,.62f,.60f,.018f});
    }
    Interface(r);
    r.End();
}

void Game::Interface(Renderer& r) {
    r.Rect(0,0,1280,94,{.035f,.055f,.065f,.94f});
    r.Rect(30,92,1220,1,{.67f,.56f,.34f,.5f});
    r.Text(32,20,"남겨진 불씨",Paper,2.4f);
    r.Text(34,57,"GSE01 / 하나의 삶이 다른 삶을 남긴다",Gold,1.5f);
    std::string region=Distance(player_,{0,0})<8?"불씨 쉼터":"적막의 숲";
    r.Text(855,24,region,Paper,2);
    r.Text(855,52,"삶 "+std::to_string(nextLife_)+" / 이어진 삶 "+std::to_string(heirs_.size()),Gold,1.5f);

    r.Rect(30,118,235,84,{.035f,.055f,.065f,.85f});
    r.Rect(30,118,3,84,Gold);
    r.Text(45,132,"작은 친절",Gold,1.5f);
    r.Text(45,155,gift_?"품에 간직한 나무 새":"불지기를 만나 보세요",Paper,1.5f);
    r.Text(45,177,"기억한 장소 "+std::to_string(discoveries_.size()),{.53f,.68f,.64f},1.5f);

    // Local compass map; icons represent actual nearby world objects.
    r.Rect(1110,117,140,140,{.035f,.055f,.065f,.88f});
    r.Text(1125,128,"주변",Gold,1.5f);
    r.Line({1125,188},{1235,188},1,{.25f,.32f,.31f});
    r.Line({1180,152},{1180,238},1,{.25f,.32f,.31f});
    auto marker=[&](WorldPoint p,Color color,float size) {
        float x=1180+static_cast<float>((p.x-player_.x)*4);
        float y=192+static_cast<float>((p.y-player_.y)*4);
        if(x>1118 && x<1242 && y>151 && y<245) Diamond(r,{x,y},size,size,color);
    };
    marker({0,0},Gold,4);
    for(const auto& h:heirs_) if(Distance(h.p,player_)<22) marker(h.p,{.63f,.56f,.79f},3);
    marker(player_,Paper,3);
    r.Text(1116,270,"불씨 / 나 / 전승",{.59f,.66f,.62f},1.2f);

    if(messageTime_>0) {
        r.Rect(155,612,970,86,{.025f,.04f,.05f,.95f});
        r.Rect(155,612,3,86,Gold);
        r.Text(177,628,message_,Paper,1.5f);
    }
    r.Rect(0,731,1280,69,{.035f,.055f,.065f,.96f});
    r.Rect(30,731,1220,1,{.67f,.56f,.34f,.5f});
    r.Text(32,748,"WASD / 방향키  이동     Shift  달리기     E  대화·조사     K  삶의 전승",Paper,1.5f);
    r.Text(32,776,"F3  청크 표시     F5  저장     Esc  저장 후 종료",{.49f,.59f,.57f},1.3f);
    if(saveBlocked_) r.Text(851,776,"저장 불가",{.94f,.49f,.34f},1.3f);
    else r.Text(851,776,"10초마다 자동 저장",Gold,1.3f);
    if(debug_) {
        TileKey c=ChunkAt(player_);
        r.Rect(30,218,450,50,{0,0,0,.7f});
        r.Text(42,230,"청크 "+std::to_string(c.first)+" : "+std::to_string(c.second),Paper,1.5f);
        r.Text(42,250,"불러온 청크 "+std::to_string(chunks_.size())+" / 시드 20260908",Gold,1.5f);
    }
    if(deathPrompt_) {
        r.Rect(0,0,1280,800,{.01f,.02f,.03f,.78f});
        r.Rect(260,269,760,238,{.07f,.09f,.10f});
        r.Rect(260,269,760,2,Gold);
        r.Text(303,291,"하나의 죽음, 하나의 새 생명.",Gold,2.5f);
        r.Text(303,348,"당신이 베푼 마음과 걸어온 길이 새로운 존재에게 이어집니다.",Paper,1.8f);
        r.Text(303,380,"이번 시연에서는 모닥불 곁에서 다음 삶을 시작합니다.",Paper,1.8f);
        r.Text(303,449,"Enter  삶을 잇기          Esc  더 살아가기",Gold,2);
    }
}

void Game::Message(const std::string& text) { message_=text; messageTime_=10; }
void Game::Interact() {
    const Heir* nearest=nullptr;
    double nearestDistance=1.7;
    for(const auto& h:heirs_) {
        double d=Distance(player_,h.p);
        if(d<nearestDistance) {nearest=&h;nearestDistance=d;}
    }
    if(nearest) {
        std::string id="이어진 삶 "+std::to_string(nearest->id)+" - ";
        Message(id+(nearest->kindness>0?"다정함은 남아서\n불가에 자리를 하나 비워 뒀어요. 누군가 추워할 것 같아서요.":
            nearest->distance>20?"길이 부르는 곳\n가 본 적 없는 길이 자꾸 꿈에 나와요. 언젠가 함께 걸어 줄래요?":
            "고요한 마음\n이상하죠. 처음 온 곳인데... 오래 머물렀던 집 같아요."));
        return;
    }
    if(Distance(player_,{-.8,1.2})<1.9) {
        bool firstKindness=kindness_==0;
        kindness_=1;
        if(!gift_) {
            gift_=true;
            Message("불지기 — 온기를 찾아왔구나.\n이 나무 새를 가져가렴. 누군가는 집까지 데려가 줘야지.");
            Save();
        } else {
            Message("불지기 — 그 작은 새, 아직 가지고 있구나.\n잘됐네. 불 옆자리는 너 앉으라고 남겨 뒀어.");
            if(firstKindness) Save();
        }
        return;
    }
    auto examine=[&](const Object& o) {
        if(o.kind!=Kind::Shrine || Distance(player_,o.p)>1.7) return false;
        if(discoveries_.insert(Tile(o.p)).second) {
            Message("이름 없는 기념비 곁에 작은 꽃이 뿌리를 내립니다.\n이곳은 당신이 다녀간 일을 기억할 것입니다."); Save();
        } else Message("꽃은 아직 이 자리에 있습니다.\n우리가 떠나도, 어떤 것들은 남습니다.");
        return true;
    };
    for(const auto& o:village_) if(examine(o)) return;
    for(const auto& c:chunks_) for(const auto& o:c.second.objects) if(examine(o)) return;
    Message("조금 더 가까이 다가가 보세요.\n불지기나 이어진 삶, 희미하게 빛나는 기념비와 이야기를 나눌 수 있습니다.");
}
void Game::Action(unsigned char key) {
    if(deathPrompt_) {
        if(key==27) deathPrompt_=false;
        if(key==13) {
            if(saveBlocked_) { deathPrompt_=false; Message("삶을 전승하려면 먼저 저장할 수 있어야 합니다.\n기존 기록은 그대로 보존되어 있습니다."); return; }
            WorldPoint oldPlayer=player_; int oldKindness=kindness_; double oldDistance=distance_;
            heirs_.push_back({nextLife_,player_,kindness_,distance_});
            ++nextLife_; player_={1.5,1.5}; kindness_=0; distance_=0;
            // Commit the entire transition in one atomic replacement; roll back on failure.
            if(!Save()) {
                --nextLife_; heirs_.pop_back(); player_=oldPlayer;
                kindness_=oldKindness; distance_=oldDistance;
            } else {
                camera_=player_; Stream();
                Message("당신의 발걸음이 멎은 곳에서 새로운 삶이 눈을 뜹니다.\n그곳으로 돌아가 보세요. 당신의 무언가가 남아 있습니다.");
            }
            deathPrompt_=false;
        }
        return;
    }
    if(key=='e' || key=='E') Interact();
    if(key=='k' || key=='K') deathPrompt_=true;
    if(key=='g') debug_=!debug_; // Internal dispatch for F3.
    if(key=='p') { if(Save()) Message("당신의 발걸음과 세계의 기억을 저장했습니다."); }
}

bool Game::Save() {
    if(saveBlocked_ || savePath_.empty()) return false;
    std::wstring temp=savePath_+L".tmp";
    std::ofstream file(temp.c_str(),std::ios::trunc);
    file << "GSE01_PROTO 1\n" << std::setprecision(17)
         << player_.x << ' ' << player_.y << ' ' << nextLife_ << ' ' << kindness_ << ' '
         << distance_ << ' ' << gift_ << '\n' << heirs_.size() << '\n';
    for(const auto& h:heirs_) file << h.id << ' ' << h.p.x << ' ' << h.p.y << ' '
                                 << h.kindness << ' ' << h.distance << '\n';
    file << discoveries_.size() << '\n';
    for(const auto& p:discoveries_) file << p.first << ' ' << p.second << '\n';
    file.flush(); bool ok=file.good(); file.close(); ok=ok && !file.fail();
    if(!ok || !MoveFileExW(temp.c_str(),savePath_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        Message("저장하지 못했습니다. 저장 공간과 폴더 권한을 확인해 주세요.\n이전 저장 파일은 그대로 보존했습니다.");
        std::cerr << "Prototype save failed.\n"; return false;
    }
    return true;
}
bool Game::Load() {
    DWORD attributes=GetFileAttributesW(savePath_.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES) {
        DWORD error=GetLastError();
        return error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND;
    }
    std::ifstream file(savePath_.c_str());
    std::string magic; int version=0,kindness=0,gift=0; WorldPoint position;
    std::uint64_t life=0; double travelled=0; std::size_t count=0;
    if(!(file>>magic>>version) || magic!="GSE01_PROTO" || version!=1) return false;
    if(!(file>>position.x>>position.y>>life>>kindness>>travelled>>gift)) return false;
    if(!ValidPoint(position) || life<1 || kindness<0 || kindness>1 || gift<0 || gift>1 ||
       !std::isfinite(travelled) || travelled<0) return false;
    if(!(file>>count) || count>100000 || life!=count+1) return false;
    std::vector<Heir> heirs;
    for(std::size_t i=0;i<count;++i) {
        Heir h;
        if(!(file>>h.id>>h.p.x>>h.p.y>>h.kindness>>h.distance) || h.id!=i+1 ||
           !ValidPoint(h.p) || h.kindness<0 || h.kindness>1 || !std::isfinite(h.distance) || h.distance<0) return false;
        heirs.push_back(h);
    }
    if(!(file>>count) || count>1000000) return false;
    std::set<TileKey> discoveries;
    for(std::size_t i=0;i<count;++i) {
        TileKey key;
        if(!(file>>key.first>>key.second) ||
           !ValidPoint({double(key.first),double(key.second)}) || !discoveries.insert(key).second) return false;
    }
    file>>std::ws;
    if(!file.eof()) return false;
    player_=position; nextLife_=life; kindness_=kindness; distance_=travelled; gift_=gift!=0;
    heirs_=std::move(heirs); discoveries_=std::move(discoveries);
    Message("돌아왔군요. 불씨는 아직 꺼지지 않았습니다.\n이어진 삶과 발견의 기록을 불러왔습니다.");
    return true;
}

