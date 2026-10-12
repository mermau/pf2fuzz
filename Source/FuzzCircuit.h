/*
  FuzzCircuit.h  —  modello fisico del Shin-Ei Companion Fuzz FY-2
  ---------------------------------------------------------------------------
  Riferimento: schema GGG "SECF" (replica FY-2) senza le aggiunte moderne
  (booster, LED, R11 anti-pop); topologia identica al kit FuzzDog FY2 V3.
  Indipendente da JUCE. Ingresso e uscita in VOLT.

  L'intero circuito è risolto ad ogni campione:
   - analisi nodale modificata (12 nodi), condensatori discretizzati con la regola
     dei trapezi (equivalente alla trasformata bilineare);
   - Q1 e Q2 con modello di Ebers-Moll completo (giunzioni base-emettitore e
     base-collettore, β diretto e inverso) e capacità Cμ;
   - le non linearità sono ridotte alle 4 tensioni di giunzione (metodo "K" /
     DK): ad ogni campione si risolve un sistema 4×4 con Newton-Raphson.

  Così emergono da sole, senza costanti empiriche: la polarizzazione (Q1 ≈ 6.6 V,
  Q2 ≈ 2.0 V come nelle misure GGG), il clipping asimmetrico, lo spostamento di
  polarizzazione per la carica di C3 (gating), l'interazione tra gli stadi e il
  carico della rete d'uscita sui collettori.

   pick-up (R + L) ─ cavo ─ IN ─ C1 ─┬─ Q1 base          R1‖C2: collettore→base di Q1, R2 da Vcc
                 Q1 coll ─┬─ C3 ─ Q2 base   R3: collettore→base di Q2
                          └─ C4 ─ cursore FUZZ
   Q2 coll (R4 → nodo S, R9‖C8 da Vcc) ─ C5 ─ estremo FUZZ
   altro estremo FUZZ ─ C6/R5/R6/C7 ─ VOLUME ─ OUT (carico ampli 1 MΩ)
*/
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace pf2
{
constexpr double kPi = 3.14159265358979323846;

//==============================================================================
struct Components
{
    static constexpr double Vcc = 9.0;
    static constexpr double Ccable = 470e-12;   // cavo di circa 4-5 m
    static constexpr double C1 = 47e-9,  R1 = 2.2e6, C2 = 1e-9,   R2 = 22e3;
    static constexpr double C3 = 47e-9,  R3 = 1.2e6, R4 = 47e3,   R9 = 100e3, C8 = 47e-9;
    static constexpr double C4 = 2.2e-9, C5 = 3.3e-9, Rfuzz = 50e3;
    static constexpr double C6 = 1e-9,   R5 = 10e3,  R6 = 15e3,   C7 = 100e-9;
    static constexpr double Rvol = 50e3, Rload = 1e6;
    static constexpr double Cmu = 4e-12;    // capacità base-collettore dei transistor
};

// Pick-up: resistenza in continua e induttanza (valori tipici misurati)
struct Pickup
{
    double R, L;
    static constexpr Pickup ideal()      { return { 10e3, 1e-6 }; }   // sorgente resistiva
    static constexpr Pickup singleCoil() { return { 6e3,  2.5 }; }    // tipo Strat/Tele
    static constexpr Pickup p90()        { return { 8e3,  6.0 }; }    // P-90 tipico
    static constexpr Pickup humbucker()  { return { 10e3, 5.0 }; }
};

// Transistor NPN a basso guadagno (C536F7G originali non reperibili): parametri
// scelti per riprodurre le tensioni misurate da GGG
struct BJT
{
    static constexpr double Is = 1e-14, betaF = 38.0, betaR = 2.0, Vt = 0.02585;
};

//==============================================================================
class FuzzCircuit
{
public:
    enum Node { IN = 0, B1, K1, B2, K2, S, W, D, X, M, VO, WV, PU, NN };
    static constexpr int NP = 4;            // Vbe1, Vbc1, Vbe2, Vbc2

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        dcBlock.setCutoff (5.0, fs);
        fuzzPos = 0.7; volPos = 0.5;
        solveOperatingPoint();
    }

    void reset() { solveOperatingPoint(); }

    // pick-up della chitarra (vedi struct Pickup)
    void setPickup (Pickup pu)
    {
        if (std::abs (pu.R - pickup.R) < 1e-3 && std::abs (pu.L - pickup.L) < 1e-9) return;
        pickup = { std::max (pu.R, 100.0), std::max (pu.L, 1e-6) };
        build (true);
    }

    // a frequenza di controllo, valori 0..1 già smussati
    void setControls (float fuzz, float vol)
    {
        if (std::abs (fuzz - fuzzPos) < 1e-5 && std::abs (vol - volPos) < 1e-5) return;
        fuzzPos = std::clamp ((double) fuzz, 0.0, 1.0);
        volPos  = std::clamp ((double) vol,  0.0, 1.0);
        build (true);
    }

    inline double process (double vin)
    {
        // termine noto: generatore, alimentazione, storia dei condensatori
        std::array<double, NN> b = bConst;
        b[PU] += vin * gIn;
        b[PU] -= indH;   // storia dell'induttanza PU → IN
        b[IN] += indH;
        for (auto& c : caps)
        {
            if (c.i >= 0) b[(size_t) c.i] += c.J + (c.j == RAIL ? c.geq * Components::Vcc : 0.0);
            if (c.j >= 0) b[(size_t) c.j] -= c.J;
        }

        std::array<double, NP> p0 {};
        for (size_t r = 0; r < NP; ++r)
        {
            double s = 0.0;
            for (size_t k = 0; k < NN; ++k) s += P[r][k] * b[k];
            p0[r] = s;
        }

        std::array<double, NP> cur {};
        newton (p0, cur, 12);

        for (size_t r = 0; r < NN; ++r)
        {
            double s = 0.0;
            for (size_t k = 0; k < NN; ++k) s += Ainv[r][k] * b[k];
            for (size_t k = 0; k < NP; ++k) s -= Q[r][k] * cur[k];
            v[r] = s;
        }

        for (auto& c : caps) c.J = 2.0 * c.geq * capVoltage (c) - c.J;
        {
            const double vl = v[PU] - v[IN];
            const double il = indG * vl + indH;
            indH = il + indG * vl;
        }

        return dcBlock.hp (v[WV]);
    }

    // per test e diagnostica: tensioni continue ai collettori
    double nodeVoltage (int n) const { return v[(size_t) n]; }

private:
    static constexpr int GND = -1, RAIL = -2;
    struct Res { int i, j; double r; };
    struct Cap { int i, j; double C, geq = 0.0, J = 0.0; };

    std::array<Cap, 10> caps { {
        { IN, GND, Components::Ccable },
        { IN, B1, Components::C1 }, { K1, B1, Components::C2 }, { K1, B2, Components::C3 },
        { S, RAIL, Components::C8 }, { K1, W, Components::C4 },  { K2, D, Components::C5 },
        { X, VO, Components::C6 },   { M, GND, Components::C7 },  { K2, B2, Components::Cmu } } };

    double capVoltage (const Cap& c) const
    {
        const double vi = c.i >= 0 ? v[(size_t) c.i] : 0.0;
        const double vj = c.j >= 0 ? v[(size_t) c.j] : (c.j == RAIL ? Components::Vcc : 0.0);
        return vi - vj;
    }

    //--------------------------------------------------------------------------
    void build (bool withCaps)
    {
        using C = Components;
        std::array<std::array<double, NN>, NN> A {};
        bConst.fill (0.0);

        auto addG = [&] (int i, int j, double g)
        {
            if (i >= 0) A[(size_t) i][(size_t) i] += g;
            if (j >= 0) A[(size_t) j][(size_t) j] += g;
            if (i >= 0 && j >= 0) { A[(size_t) i][(size_t) j] -= g; A[(size_t) j][(size_t) i] -= g; }
            if (j == RAIL && i >= 0) bConst[(size_t) i] += g * C::Vcc;
        };
        auto addR = [&] (int i, int j, double r) { addG (i, j, 1.0 / std::max (r, 1.0)); };

        // generatore del pick-up (equivalente di Norton) e induttanza PU → IN
        gIn = 1.0 / pickup.R;
        addG (PU, GND, gIn);
        indG = withCaps ? 1.0 / (2.0 * fs * pickup.L) : 1.0;   // in continua: cortocircuito
        addG (PU, IN, indG);
        addR (K1, B1, C::R1);  addR (K1, RAIL, C::R2);
        addR (K2, B2, C::R3);  addR (K2, S, C::R4);   addR (S, RAIL, C::R9);
        // FUZZ al massimo = cursore verso l'uscita (più contributo diretto di Q1, più
        // armoniche nelle misure del modello); al minimo = cursore verso C5
        addR (D, W, fuzzPos * C::Rfuzz);              addR (W, X, (1.0 - fuzzPos) * C::Rfuzz);
        addR (X, M, C::R5);    addR (VO, M, C::R6);
        addR (VO, WV, (1.0 - volPos) * C::Rvol);      addR (WV, GND, volPos * C::Rvol);
        addR (WV, GND, C::Rload);

        for (auto& c : caps)
        {
            c.geq = withCaps ? 2.0 * c.C * fs : 0.0;
            addG (c.i, c.j == RAIL ? GND : c.j, c.geq);   // il binario è un riferimento fisso
        }

        invert (A, Ainv);

        // P = Np·Ainv (tensioni di giunzione), Q = Ainv·Nc^T (correnti nei nodi)
        const int bN[2] = { B1, B2 }, cN[2] = { K1, K2 };
        for (size_t k = 0; k < NN; ++k)
            for (size_t t = 0; t < 2; ++t)
            {
                P[2 * t][k]     = Ainv[(size_t) bN[t]][k];
                P[2 * t + 1][k] = Ainv[(size_t) bN[t]][k] - Ainv[(size_t) cN[t]][k];
            }
        // ordine delle correnti: Ib1, Ic1, Ib2, Ic2 (uscenti dai nodi B1, K1, B2, K2)
        const int curNode[NP] = { B1, K1, B2, K2 };
        for (size_t r = 0; r < NN; ++r)
            for (size_t k = 0; k < NP; ++k) Q[r][k] = Ainv[r][(size_t) curNode[k]];
        for (size_t r = 0; r < NP; ++r)
            for (size_t k = 0; k < NP; ++k) K[r][k] = P[r][(size_t) curNode[k]];
    }

    // Ebers-Moll: correnti e derivate di un transistor (Ib, Ic) rispetto a (Vbe, Vbc)
    static inline void ebersMoll (double vbe, double vbc, double& ib, double& ic, double d[2][2])
    {
        using T = BJT;
        const double ef = std::exp (std::min (vbe, 1.2) / T::Vt);
        const double er = std::exp (std::min (vbc, 1.2) / T::Vt);
        ic = T::Is * (ef - er) - T::Is / T::betaR * (er - 1.0);
        ib = T::Is / T::betaF * (ef - 1.0) + T::Is / T::betaR * (er - 1.0);
        d[0][0] = T::Is / T::betaF * ef / T::Vt;          // dIb/dVbe
        d[0][1] = T::Is / T::betaR * er / T::Vt;          // dIb/dVbc
        d[1][0] = T::Is * ef / T::Vt;                     // dIc/dVbe
        d[1][1] = -T::Is * er / T::Vt * (1.0 + 1.0 / T::betaR); // dIc/dVbc
    }

    // Risolve p = p0 - K·i(p) con Newton-Raphson (passo limitato)
    void newton (const std::array<double, NP>& p0, std::array<double, NP>& cur, int maxIter)
    {
        auto& p = pState;
        for (int it = 0; it < maxIter; ++it)
        {
            double d1[2][2], d2[2][2];
            ebersMoll (p[0], p[1], cur[0], cur[1], d1);
            ebersMoll (p[2], p[3], cur[2], cur[3], d2);

            std::array<double, NP> F {};
            for (size_t r = 0; r < NP; ++r)
            {
                double s = p0[r] - p[r];
                for (size_t k = 0; k < NP; ++k) s -= K[r][k] * cur[k];
                F[r] = s;
            }

            // J = -I - K·Di  (Di è diagonale a blocchi 2×2)
            double J[NP][NP + 1];
            for (size_t r = 0; r < NP; ++r)
            {
                for (size_t c = 0; c < 2; ++c)
                {
                    J[r][c]     = -(K[r][0] * d1[0][c] + K[r][1] * d1[1][c]);
                    J[r][c + 2] = -(K[r][2] * d2[0][c] + K[r][3] * d2[1][c]);
                }
                J[r][r] -= 1.0;
                J[r][NP] = -F[r];
            }
            solve4 (J);

            double maxStep = 0.0;
            for (size_t r = 0; r < NP; ++r) maxStep = std::max (maxStep, std::abs (J[r][NP]));
            const double scale = maxStep > 0.1 ? 0.1 / maxStep : 1.0;
            for (size_t r = 0; r < NP; ++r) p[r] += scale * J[r][NP];
            if (maxStep < 1e-10) break;
        }
        double dd[2][2];
        ebersMoll (p[0], p[1], cur[0], cur[1], dd);
        ebersMoll (p[2], p[3], cur[2], cur[3], dd);
    }

    static void solve4 (double J[NP][NP + 1])
    {
        for (size_t c = 0; c < NP; ++c)
        {
            size_t piv = c;
            for (size_t r = c + 1; r < NP; ++r) if (std::abs (J[r][c]) > std::abs (J[piv][c])) piv = r;
            if (piv != c) for (size_t k = 0; k <= NP; ++k) std::swap (J[c][k], J[piv][k]);
            const double inv = 1.0 / J[c][c];
            for (size_t r = c + 1; r < NP; ++r)
            {
                const double f = J[r][c] * inv;
                for (size_t k = c; k <= NP; ++k) J[r][k] -= f * J[c][k];
            }
        }
        for (size_t c = NP; c-- > 0;)
        {
            double s = J[c][NP];
            for (size_t k = c + 1; k < NP; ++k) s -= J[c][k] * J[k][NP];
            J[c][NP] = s / J[c][c];
        }
    }

    static void invert (std::array<std::array<double, NN>, NN> a, std::array<std::array<double, NN>, NN>& inv)
    {
        for (size_t r = 0; r < NN; ++r) { inv[r].fill (0.0); inv[r][r] = 1.0; }
        for (size_t c = 0; c < NN; ++c)
        {
            size_t p = c;
            for (size_t r = c + 1; r < NN; ++r) if (std::abs (a[r][c]) > std::abs (a[p][c])) p = r;
            std::swap (a[c], a[p]); std::swap (inv[c], inv[p]);
            const double d = 1.0 / a[c][c];
            for (size_t k = 0; k < NN; ++k) { a[c][k] *= d; inv[c][k] *= d; }
            for (size_t r = 0; r < NN; ++r)
                if (r != c)
                {
                    const double f = a[r][c];
                    for (size_t k = 0; k < NN; ++k) { a[r][k] -= f * a[c][k]; inv[r][k] -= f * inv[c][k]; }
                }
        }
    }

    // Punto di lavoro in continua (condensatori aperti), poi inizializza la storia
    void solveOperatingPoint()
    {
        build (false);
        pState = { 0.6, -6.0, 0.6, -1.4 };
        std::array<double, NN> b = bConst;
        std::array<double, NP> p0 {}, cur {};
        for (size_t r = 0; r < NP; ++r)
        {
            double s = 0.0;
            for (size_t k = 0; k < NN; ++k) s += P[r][k] * b[k];
            p0[r] = s;
        }
        newton (p0, cur, 200);
        for (size_t r = 0; r < NN; ++r)
        {
            double s = 0.0;
            for (size_t k = 0; k < NN; ++k) s += Ainv[r][k] * b[k];
            for (size_t k = 0; k < NP; ++k) s -= Q[r][k] * cur[k];
            v[r] = s;
        }
        build (true);
        for (auto& c : caps) c.J = c.geq * capVoltage (c);   // corrente nulla a regime
        indH = indG * (v[PU] - v[IN]);                           // corrente nulla (v ≈ 0 in continua)
        dcBlock.reset();
    }

    struct OnePole
    {
        double g = 0.0, s = 0.0;
        void reset() { s = 0.0; }
        void setCutoff (double fc, double sr) { const double w = std::tan (kPi * fc / sr); g = w / (1.0 + w); }
        inline double hp (double x) { const double u = (x - s) * g; const double y = u + s; s = y + u; return x - y; }
    };

    double fs = 192000.0, fuzzPos = 0.7, volPos = 0.5, gIn = 0.0, indG = 0.0, indH = 0.0;
    Pickup pickup = Pickup::singleCoil();
    std::array<std::array<double, NN>, NN> Ainv {};
    std::array<std::array<double, NN>, NP> P {};
    std::array<std::array<double, NP>, NN> Q {};
    std::array<std::array<double, NP>, NP> K {};
    std::array<double, NN> bConst {}, v {};
    std::array<double, NP> pState { 0.6, -6.0, 0.6, -1.4 };
    OnePole dcBlock;
};

} // namespace pf2
