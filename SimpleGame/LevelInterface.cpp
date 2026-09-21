#include "stdafx.h"
#include "LevelOne.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace
{

    const Color Paper(.85f, .86f, .78f), Gold(.85f, .65f, .35f), Ink(.045f, .06f, .07f);
}

void LevelOne::DrawInterface(Renderer& r)
{
    Point entrance = Screen({4.5, 4.5});
    r.Rect(0, 0, 1280, 107, {.025f, .04f, .05f, .95f});
    r.Text(28, 14, "레벨 1 · 잿빛 수확지", Paper, 2.2f);
    r.Text(28, 47, "자동 조준 · 자동 발사 / 사거리 안에서 적을 상대하세요", Gold, 1.4f);
    r.Rect(28, 82, 270, 10, {.2f, .13f, .13f});
    r.Rect(28, 82, 270 * hp_ / MaxHP(), 10, {.7f, .24f, .18f});
    r.Text(315,
           74,
           "체력 " + std::to_string(int(hp_)) + " / " + std::to_string(int(MaxHP())),
           Paper,
           1.3f);
    std::ostringstream stats;
    stats << "레벨 " << level_ << " / 강화 +" << weapon_ << " / 피해 " << int(Damage())
          << " / 간격 " << std::fixed << std::setprecision(2) << Cooldown() << "초";
    r.Text(590, 16, stats.str(), Paper, 1.35f);
    r.Text(590,
           46,
           "경험치 " + std::to_string(xp_) + " / " + std::to_string(RequiredXP()) + " / 처치 " +
               std::to_string(kills_) + " / 습득 " + std::to_string(picked_),
           Gold,
           1.3f);
    r.Rect(590, 83, 580, 7, Ink);
    r.Rect(590, 83, 580 * float(xp_) / RequiredXP(), 7, {.25f, .64f, .66f});
    std::string goal = cleared_       ? "완료: 입구로 돌아가 L로 귀환"
                       : kills_ == 0  ? "1. 적을 처치해 경험치와 드랍 획득"
                       : weapon_ == 0 ? "2. 노란 강화석을 자동 습득"
                       : level_ < 3   ? "3. 영혼석을 모아 레벨 3 달성"
                       : kills_ < 18  ? "4. 일반 적 18마리 처치"
                                      : "5. 재의 파수꾼 처치";
    r.Rect(22, 121, 510, 36, {.025f, .04f, .05f, .85f});
    r.Text(34, 126, goal, Paper, 1.4f);
    for (const auto& e : Enemies())
    {
        if (e.type == 2)
        {
            r.Rect(640, 122, 600, 38, {.025f, .04f, .05f, .9f});
            r.Text(651, 123, "재의 파수꾼", Gold, 1.2f);
            r.Rect(650, 149, 580, 5, {.2f, .1f, .1f});
            r.Rect(650, 149, 580 * std::max(0.f, e.hp) / 480, 5, {.8f, .2f, .12f});
        }
    }
    if (CanLeave())
    {
        r.Text(entrance.x - 35, entrance.y - 30, "L  마을 귀환", Paper, 1.5f);
    }
    if (noticeTime_ > 0)
    {
        r.Rect(75, 666, 1130, 39, {.025f, .04f, .05f, .9f});
        r.Text(89, 673, notice_, Paper, 1.4f);
    }
    r.Rect(0, 733, 1280, 67, {.025f, .04f, .05f, .95f});
    r.Text(28,
           742,
           "WASD 이동 / Shift 달리기 / L 입구 귀환 / F5 저장 / F6~F9 후처리 / Esc 저장 후 종료",
           Paper,
           1.25f);
    r.Text(28,
           773,
           "청록: 영혼석(경험치) / 노랑: 무기 강화 / 붉은색: 체력 회복 / 가까이 가면 자동 습득",
           Gold,
           1.2f);
    if (Dead())
    {
        r.Rect(0, 0, 1280, 800, {0, 0, 0, .7f});
        r.Text(280, 335, "쓰러졌습니다. 하지만 삶은 이어집니다.", Gold, 2.2f);
        r.Text(280,
               390,
               "Enter: 전승 후 마을 귀환 / 실패 시 Enter 재시도 / Esc 저장 후 종료",
               Paper,
               1.4f);
        if (noticeTime_ > 0)
        {
            r.Text(140, 460, notice_, Paper, 1.25f);
        }
    }
}
