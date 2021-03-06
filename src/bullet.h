#ifndef BULLET_H
#define BULLET_H

#include "types.h"
#include "list.h"
#include "sprite.h"
#include "game.h"
#include "common/unsorted_vector.h"
#include "common/claimable.h"
#include <tuple>
#include <array>

void __fastcall RemoveFromBulletTargets(Unit *unit);
// Returns true if there's need for UnitWasHit processing
bool UnitWasHit(Unit *target, Unit *attacker, bool notify);

Unit **FindNearbyHelpingUnits(Unit *unit, TempMemoryPool *allocation_pool);

// Prefer DamagedUnit::AddHit or add more functionality to DamagedUnit as AddHit requires weapon_id
// This is only for bw compatibility, or if you have really good reason to deal damage outside
// BulletSystem::ProgressFrames(). It is an
void DamageUnit(int damage, Unit *target, vector<Unit *> *killed_units);

void HallucinationHit(Unit *target, Unit *attacker, int direction);
// Passing -1 as attacking_player is allowed to indicate no attacker
void IncrementKillScores(Unit *target, int attacking_player);

int GetWeaponDamage(const Unit *target, int weapon_id, int player);

extern TempMemoryPool pbf_memory;
extern bool bulletframes_in_progress;
const int CallFriends_Radius = 0x60;

namespace Weapon
{
    const int SpiderMine = 0x06;
    const int Lockdown = 0x20;
    const int EmpShockwave = 0x21;
    const int Irradiate = 0x22;
    const int Venom = 0x32;
    const int HeroVenom = 0x33;
    const int Suicide = 0x36;
    const int Parasite = 0x38;
    const int SpawnBroodlings = 0x39;
    const int Ensnare = 0x3a;
    const int DarkSwarm = 0x3b;
    const int Plague = 0x3c;
    const int Consume = 0x3d;
    const int PsiAssault = 0x44;
    const int HeroPsiAssault = 0x45;
    const int Scarab = 0x52;
    const int StasisField = 0x53;
    const int PsiStorm = 0x54;
    const int Restoration = 0x66;
    const int MindControl = 0x69;
    const int Feedback = 0x6a;
    const int OpticalFlare = 0x6b;
    const int Maelstrom = 0x6c;
    const int SubterraneanSpines = 0x6d;
    const int None = 0x82;
}

enum class BulletState
{
    Init,
    MoveToPoint,
    MoveToTarget,
    Bounce,
    Die,
    MoveNearUnit,
    GroundDamage
};

struct SpellCast
{
    SpellCast(int pl, const Point &p, int t, Unit *pa) : player(pl), tech(t), pos(p), parent(pa) {}
    uint16_t player;
    uint16_t tech;
    Point pos;
    Unit *parent;
};

struct BulletHit
{
    BulletHit(Unit *a, Bullet *b, int32_t d) : target(a), bullet(b), damage(d) {}
    Unit *target;
    Bullet *bullet;
    int32_t damage;
};

struct BulletFramesInput
{
    BulletFramesInput(vector<DoWeaponDamageData> w, vector<HallucinationHitData> h, Ai::HelpingUnitVec hu) :
        weapon_damages(move(w)), hallucination_hits(move(h)), helping_units(move(hu)) {}
    BulletFramesInput(const BulletFramesInput &other) = delete;
    BulletFramesInput(BulletFramesInput &&other) = default;
    BulletFramesInput& operator=(const BulletFramesInput &other) = delete;
    BulletFramesInput& operator=(BulletFramesInput &&other) = default;
    vector<DoWeaponDamageData> weapon_damages;
    vector<HallucinationHitData> hallucination_hits;
    Ai::HelpingUnitVec helping_units;
};

struct ProgressBulletBufs;

// This class carries some extra information which is used for handling unit damage.
// Only one DamagedUnit should exist per Unit damaged in this frame.
// AddHit() does what bw function DoWeaponDamage does, with some additions which allow
// simpler code in Ai::UnitWasHit. Ideally UnitWasHit should be called only once per unit,
// and DamagedUnit deciding the most dangerous of the current attackers etc. Currently it does
// not do that though. See poorly named BulletSystem::ProcessHits() (todo refactor that) for what is done currently.
// There is also function DamageUnit() which should only be used when there is no attacker
// (And preferably not even then, but otherwise burning/plague/etc damage would have to be passed to
// BulletSystem::ProgressFrames(). The dead units are still passed there, so it might not even be worth it)
class DamagedUnit
{
    public:
        DamagedUnit(Unit *b) : base(b) {}
        Unit *base;

        bool IsDead() const;
        void AddHit(uint32_t dmg, int weapon_id, int player, int direction, Unit *attacker, ProgressBulletBufs *bufs);

    private:
        void AddDamage(int dmg);
};

struct ProgressBulletBufs
{
    ProgressBulletBufs(vector<DamagedUnit> *a, vector<tuple<Unit *, Unit *>> *b,
        vector<tuple<Unit *, Unit *, bool>> *c, vector<Unit *> *d) :
        unit_was_hit(b), ai_react(c), killed_units(d), damaged_units(a) {}
    ProgressBulletBufs(ProgressBulletBufs &&other) = default;
    ProgressBulletBufs& operator=(ProgressBulletBufs &&other) = default;

    vector<tuple<Unit *, Unit *>> *unit_was_hit;
    vector<tuple<Unit *, Unit *, bool>> *ai_react;
    vector<Unit *> *killed_units;
    vector<DamagedUnit> *damaged_units;
    // Don't call help always false
    void AddToAiReact(Unit *unit, Unit *attacker, bool main_target_reactions);

    /// Returns DamagedUnit handle corresponding to the input unit.
    /// Handle is valid during the ongoing ProgressBulletFrames call,
    /// during which the same handle is always returned for unit
    /// Current implementation just stores the data in unit struct with the frame it is valid for,
    /// but "ideal" implementation would be some kind of Unit * -> DamagedUnit map.
    /// As such calling another ProgressBulletFrames during same frame will not behave as expected,
    /// even if there were completely separate BulletSystem
    DamagedUnit GetDamagedUnit(Unit *unit);
    vector<DamagedUnit> *DamagedUnits() { return damaged_units; }
};


#pragma pack(push)
#pragma pack(1)

class Bullet
{
    friend class BulletSystem; // BulletSystem is only allowed to create bullets
    public:
        RevListEntry<Bullet, 0x0> list;
        uint32_t hitpoints;
        ptr<Sprite> sprite;

        Point move_target;
        Unit *move_target_unit;

        Point next_move_waypoint;
        Point unk_move_waypoint;

        // 0x20
        uint8_t flingy_flags;
        uint8_t facing_direction;
        uint8_t flingyTurnRadius;
        uint8_t movement_direction;
        uint16_t flingy_id;
        uint8_t _unknown_0x026;
        uint8_t flingyMovementType;
        Point position;
        Point32 exact_position;
        uint32_t flingyTopSpeed;
        int32_t current_speed;
        int32_t next_speed;

        int32_t speed[2]; // 0x40
        uint16_t acceleration;
        uint8_t pathing_direction;
        uint8_t unk4b;
        uint8_t player;
        uint8_t order;
        uint8_t order_state;
        uint8_t order_signal;
        uint16_t order_fow_unit;
        uint16_t unused52;
        uint8_t order_timer;
        uint8_t ground_cooldown;
        uint8_t air_cooldown;
        uint8_t spell_cooldown;
        Point order_target_pos;
        Unit *target;

        uint8_t weapon_id;
        uint8_t time_remaining;
        uint8_t flags;
        uint8_t bounces_remaining;
        Unit *parent; // 0x64
        Unit *previous_target;
        uint8_t spread_seed;
        uint8_t padding[0x3];


        ListEntry<Bullet, 0x70> targeting; // 0x70
        ListEntry<Bullet, 0x78> spawned; // 0x78
        uintptr_t bulletsystem_entry;

        void SingleDelete();

        void SetTarget(Unit *new_target);

        void ProgressFrame();
        // Int is how many DoMissileDmg:s happened
        tuple<BulletState, int> State_Init();
        tuple<BulletState, int> State_GroundDamage();
        // Unit * is possible new target
        tuple<BulletState, int, Unit *> State_Bounce();
        tuple<BulletState, int> State_MoveToPoint();
        tuple<BulletState, int> State_MoveToUnit();
        tuple<BulletState, int> State_MoveNearUnit();
        Optional<SpellCast> DoMissileDmg(ProgressBulletBufs *bufs);

        void UpdateMoveTarget(const Point &target);
        void Move(const Point &where);

        void Serialize(Save *save, const BulletSystem *parent);
        template <bool saving> void SaveConvert(const BulletSystem *parent);
        ~Bullet() {}

    private:
        Bullet() {}
        void NormalHit(ProgressBulletBufs *bufs);
        void HitUnit(Unit *target, int dmg, ProgressBulletBufs *bufs);
        void AcidSporeHit() const;
        template <bool air_splash> void Splash(ProgressBulletBufs *bufs, bool hit_own_units);
        void Splash_Lurker(ProgressBulletBufs *bufs);
        void SpawnBroodlingHit(vector<Unit *> *killed_units) const;

        Unit *ChooseBounceTarget();

        // Delete bullet if returns false
        bool Initialize(Unit *parent_, int player_, int direction, int weapon, const Point &pos);

        static vector<std::pair<Unit *, Point>> broodling_spawns;
        Sprite::ProgressFrame_C SetIscriptAnimation(int anim, bool force);
};

/// Contains and controls bullets of the game
/// In theory, one could have multiple completely independed BulletSystems, but obviously it would
/// require rewriting even more bw code - also ProgressBulletBufs::GetDamagedUnit has to be changed
/// if that is ever desired
class BulletSystem
{
    typedef UnsortedPtrVector<Bullet> BulletVector;
    public:
        BulletSystem() {}
        void Deserialize(Load *load);
        void Serialize(Save *save);
        template <bool saving> Bullet *SaveConvertBulletPtr(const Bullet *in) const;
        void FinishLoad();

        /// May only be called once per frame, see ProgressBulletBufs::GetDamagedUnit
        void ProgressFrames(BulletFramesInput in);
        void DeleteAll();
        Bullet *AllocateBullet(Unit *parent, int player, int direction, int weapon, const Point &pos);
        uintptr_t BulletCount()
        {
            uintptr_t count = 0;
            for (auto vec : Vectors())
                count += vec->size();
            return count;
        }

    private:
        Claimed<vector<Bullet *>> ProgressStates(vector<tuple<Bullet *, Unit *>> *new_bounce_targets);
        void ProcessHits(ProgressBulletBufs *bufs);
        vector<Unit *> ProcessUnitWasHit(vector<tuple<Unit *, Unit *>> hits, ProgressBulletBufs *bufs);
        vector<Ai::HitUnit> ProcessAiReactToHit(vector<tuple<Unit *, Unit *, bool>> input, Ai::HelpingUnitVec *helping_units);

        void DeleteBullet(Bullet *bullet);
        std::array<BulletVector *, 7> Vectors()
        {
            return { { &initstate, &moving_to_point, &moving_to_unit, &bouncing, &damage_ground, &moving_near, &dying } };
        }
        std::array<const BulletVector *, 7> Vectors() const
        {
            return { { &initstate, &moving_to_point, &moving_to_unit, &bouncing, &damage_ground, &moving_near, &dying } };
        }

        void SwitchBulletState(ptr<Bullet> &bullet, BulletState old_state, BulletState new_state);
        BulletVector *GetOwningVector(const Bullet *bullet);
        const BulletVector *GetOwningVector(const Bullet *bullet) const;
        BulletVector *GetStateVector(BulletState state);

        BulletVector initstate;
        BulletVector moving_to_point;
        BulletVector moving_to_unit;
        BulletVector bouncing;
        BulletVector damage_ground;
        BulletVector moving_near;
        BulletVector dying;
        Claimable<vector<DamagedUnit>> dmg_unit_buf;
        Claimable<vector<Bullet *>> bullet_buf;
        Claimable<vector<SpellCast>> spell_buf;
        // target, attacker
        Claimable<vector<tuple<Unit *, Unit *>>> unit_was_hit_buf;
        // target, attacker, main_target_reactions
        Claimable<vector<tuple<Unit *, Unit *, bool>>> ai_react_buf;
        Claimable<vector<Unit *>> killed_units_buf;
        // Needed as DoMissileDmg should be done with old target
        // There is previous_target as well which could have been used
        Claimable<vector<tuple<Bullet *, Unit *>>> bounce_target_buf;

    public:
        class ActiveBullets_
        {
            public:
                ActiveBullets_(BulletSystem *p) : parent(p) {}

                class iterator
                {
                    public:
                        typedef BulletVector::safe_iterator internal_iterator;
                        iterator(BulletSystem *p, bool end) : parent(p),
                                                              pos(p->Vectors().front()->safe_begin())
                        {
                            if (end)
                            {
                                vector_pos = parent->Vectors().size() - 1;
                                pos = parent->Vectors().back()->safe_end();
                            }
                            else
                            {
                                vector_pos = 0;
                                CheckEndOfVec();
                            }
                        }

                        iterator operator++()
                        {
                            if (pos != parent->Vectors()[vector_pos]->safe_end())
                                ++pos;
                            CheckEndOfVec();
                            return *this;
                        }

                        Bullet *operator*() { return (*pos).get(); }
                        bool operator==(const iterator &other) const
                        {
                            return vector_pos == other.vector_pos && pos == other.pos;
                        }
                        bool operator!=(const iterator &other) const { return !(*this == other); }

                    private:

                        void CheckEndOfVec()
                        {
                            while (pos == parent->Vectors()[vector_pos]->safe_end() &&
                                    vector_pos < parent->Vectors().size() - 1)
                            {
                                pos = parent->Vectors()[++vector_pos]->safe_begin();
                            }
                        }

                        BulletSystem *parent;
                        internal_iterator pos;
                        int vector_pos;
                };
                iterator begin() { return iterator(parent, false); }
                iterator end() { return iterator(parent, true); }

            private:
                BulletSystem *parent;
        };
        ActiveBullets_ ActiveBullets() { return ActiveBullets_(this); }
        friend class ActiveBullets_;
        friend class ActiveBullets_::iterator;
};

extern BulletSystem *bullet_system;

static_assert(sizeof(Bullet) == 0x84, "Sizeof bullet");

#pragma pack(pop)

#endif // BULLET_H

