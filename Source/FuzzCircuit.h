/*
  FuzzCircuit.h  —  modello fisico del Shin-Ei Companion Fuzz FY-2
  ---------------------------------------------------------------------------
  Riferimento: schema dell'originale Shin-ei Companion FUZZ BOX FY-2 (identico,
  nei valori, allo schema GGG "SECF" privato delle aggiunte moderne: booster,
  LED, R11 anti-pop). Pot FUZZ e VOL: Cosmos B50K (B = lineare).
  Indipendente da JUCE. Ingresso e uscita in VOLT.

  L'intero circuito è risolto ad ogni campione:
   - analisi nodale modificata (12 nodi), condensatori discretizzati con la regola
     dei trapezi (equivalente alla trasformata bilineare);
   - Q1 e Q2 con modello di Ebers-Moll completo (giunzioni base-emettitore e
     base-collettore, β diretto e inverso) e capacità Cμ;
   - le non linearità sono ridotte alle 4 tensioni di giunzione (metodo "K" /
     DK): ad ogni campione si risolve un sistema 4×4 con Newton-Raphson.

  Così emergono da sole, senza costanti empiriche: la polarizzazione (con i
  2SC536F originali Q1 ≈ 3.2 V, Q2 ≈ 0.9 V), il clipping asimmetrico, lo spostamento di
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
    static constexpr double Cmu = 3.5e-12;  // Cob del 2SC536 (datasheet, Vce 6 V)
};

// Pick-up e chitarra: resistenza in continua e induttanza della bobina, sua capacità
// propria, carico dei potenziometri di volume e tono (valori tipici)
struct Pickup
{
    double R, L, C = 150e-12, Rload = 250e3;
    bool ideal_ = false;
    static constexpr Pickup ideal()      { return { 10e3, 1e-6, 0.0,      1e12,  true }; }  // sorgente resistiva
    static constexpr Pickup singleCoil() { return { 6e3,  2.5,  120e-12,  125e3 }; }  // Strat/Tele, pot 250k
    static constexpr Pickup p90()        { return { 8e3,  6.0,  150e-12,  250e3 }; }  // pot 500k
    static constexpr Pickup humbucker()  { return { 10e3, 5.0,  180e-12,  250e3 }; }  // pot 500k
    bool operator== (const Pickup& o) const
    {
        return std::abs (R - o.R) < 1e-3 && std::abs (L - o.L) < 1e-9 && std::abs (C - o.C) < 1e-15
            && std::abs (Rload - o.Rload) < 1e-3 && ideal_ == o.ideal_;
    }
};

// Biquad da prototipo analogico (bilineare), doppia precisione
struct AnalogBiquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    void set (double n0, double n1, double n2, double d0, double d1, double d2, double fs)
    {
        const double K = 2.0 * fs, K2 = K * K;
        const double A0 = d0 + d1 * K + d2 * K2;
        b0 = (n0 + n1 * K + n2 * K2) / A0;  b1 = (2.0 * n0 - 2.0 * n2 * K2) / A0;  b2 = (n0 - n1 * K + n2 * K2) / A0;
        a1 = (2.0 * d0 - 2.0 * d2 * K2) / A0;  a2 = (d0 - d1 * K + d2 * K2) / A0;
    }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Transistor originali: NEC 2SC536, marcatura "C536 F7G" = classe di guadagno F
// (hFE 160-320 da datasheet); si usa il centro geometrico della classe.
// Nota: le tensioni misurate da GGG (Q1 6.6 V, Q2 2.0 V) valgono per la loro
// replica con transistor a basso guadagno, non per l'originale.
struct BJT
{
    static constexpr double Is = 1e-14, betaF = 226.0, betaR = 4.0, Vt = 0.02585;
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
        updateInputCorrection();
        solveOperatingPoint();
    }

    void reset() { solveOperatingPoint(); }

    // pick-up della chitarra (vedi struct Pickup)
    void setPickup (Pickup pu)
    {
        pu.R = std::max (pu.R, 100.0); pu.L = std::max (pu.L, 1e-6);
        pu.C = std::max (pu.C, 0.0);   pu.Rload = std::max (pu.Rload, 1e3);
        if (pu == pickup) return;
        pickup = pu;
        caps[0].C = Components::Ccable + pickup.C;
        updateInputCorrection();
        build (true);
    }

    // alimentazione (V): batteria 4.5-9.6 V o alimentatore 9 V. Va chiamata a frequenza di
    // controllo con un valore già smussato: un salto brusco sarebbe un transitorio reale
    void setSupply (double volts)
    {
        volts = std::clamp (volts, 1.0, 12.0);
        if (std::abs (volts - vcc) < 1e-4) return;
        vcc = volts;
        build (true);
    }
    double getSupply() const { return vcc; }

    // guadagno (hFE) dei due transistor: classe F del 2SC536 = 160-320
    void setHfe (double q1, double q2)
    {
        beta1 = std::clamp (q1, 10.0, 2000.0);
        beta2 = std::clamp (q2, 10.0, 2000.0);
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
        vin = inputCorrection.process (vin);

        // forma a variabili di stato: x = storie dei condensatori e dell'induttanza,
        // matrici precalcolate in build() (nessuna diramazione nel ciclo per campione)
        std::array<double, NP> p0;
        for (size_t r = 0; r < NP; ++r)
        {
            double acc = pc[r] + pIn[r] * vin;
            for (size_t k = 0; k < NS; ++k) acc += Px[r][k] * x[k];
            p0[r] = acc;
        }

        // punto di partenza: estrapolazione lineare dai due campioni precedenti
        std::array<double, NP> cur {};
        const auto pLast = pState;
        for (size_t r = 0; r < NP; ++r) pState[r] = std::clamp (2.0 * pState[r] - pPrev[r], -12.0, 1.0);
        newton (p0, cur, 40);
        pPrev = pLast;

        double y = yc + yu * vin;
        for (size_t k = 0; k < NS; ++k) y += yx[k] * x[k];
        for (size_t k = 0; k < NP; ++k) y += yi[k] * cur[k];

        std::array<double, NS> xn;
        for (size_t r = 0; r < NS; ++r)
        {
            double acc = d0[r] + bu[r] * vin;
            for (size_t k = 0; k < NS; ++k) acc += Ax[r][k] * x[k];
            for (size_t k = 0; k < NP; ++k) acc += Ci[r][k] * cur[k];
            xn[r] = acc;
        }
        lastX = x; lastVin = vin; lastCur = cur;
        x = xn;

        return dcBlock.hp (y);
    }

    // per test e diagnostica: tensione di un nodo all'ultimo campione calcolato
    double nodeVoltage (int n) const
    {
        const auto r = (size_t) n;
        double acc = m0[r] + mu[r] * lastVin;
        for (size_t k = 0; k < NS; ++k) acc += Mx[r][k] * lastX[k];
        for (size_t k = 0; k < NP; ++k) acc += mi[r][k] * lastCur[k];
        return acc;
    }

private:
    static constexpr int GND = -1, RAIL = -2;
    struct Res { int i, j; double r; };
    struct Cap { int i, j; double C, geq = 0.0; };
    static constexpr int NC = 10, NS = NC + 1;   // stati: 10 condensatori + induttanza del pick-up

    std::array<Cap, NC> caps { {
        { IN, GND, Components::Ccable },
        { IN, B1, Components::C1 }, { K1, B1, Components::C2 }, { K1, B2, Components::C3 },
        { S, RAIL, Components::C8 }, { K1, W, Components::C4 },  { K2, D, Components::C5 },
        { X, VO, Components::C6 },   { M, GND, Components::C7 },  { K2, B2, Components::Cmu } } };

    // tensione ai capi di un condensatore dato il vettore dei nodi
    double capVoltage (const Cap& c, const std::array<double, NN>& vn) const
    {
        const double vi = c.i >= 0 ? vn[(size_t) c.i] : 0.0;
        const double vj = c.j >= 0 ? vn[(size_t) c.j] : (c.j == RAIL ? vcc : 0.0);
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
            if (j == RAIL && i >= 0) bConst[(size_t) i] += g * vcc;
        };
        auto addR = [&] (int i, int j, double r) { addG (i, j, 1.0 / std::max (r, 1.0)); };

        // generatore del pick-up (equivalente di Norton) e induttanza PU → IN;
        // all'uscita del pick-up: carico dei pot della chitarra (capacità propria e cavo in caps[0])
        gIn = 1.0 / pickup.R;
        addG (PU, GND, gIn);
        addR (IN, GND, pickup.Rload);
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

        // termine noto: b = b0 + Bu·vin + Bx·x  (Bx: ±1 sui nodi di ogni elemento reattivo)
        std::array<double, NN> b0 = bConst;
        for (auto& c : caps) if (c.j == RAIL && c.i >= 0) b0[(size_t) c.i] += c.geq * vcc;
        auto bxCol = [this] (size_t k, size_t node) -> double
        {
            if (k < (size_t) NC)
            {
                const auto& c = caps[k];
                return (c.i == (int) node ? 1.0 : 0.0) - (c.j == (int) node ? 1.0 : 0.0);
            }
            return (node == (size_t) IN ? 1.0 : 0.0) - (node == (size_t) PU ? 1.0 : 0.0);   // induttanza
        };

        // nodi: v = m0 + mu·vin + Mx·x + mi·i
        for (size_t r = 0; r < NN; ++r)
        {
            double a0 = 0.0;
            for (size_t c = 0; c < NN; ++c) a0 += Ainv[r][c] * b0[c];
            m0[r] = a0;
            mu[r] = Ainv[r][PU] * gIn;
            for (size_t k = 0; k < NS; ++k)
            {
                double a = 0.0;
                for (size_t c = 0; c < NN; ++c) { const double bx = bxCol (k, c); if (std::abs (bx) > 0.5) a += Ainv[r][c] * bx; }
                Mx[r][k] = a;
            }
            for (size_t k = 0; k < NP; ++k) mi[r][k] = -Q[r][k];
        }

        // giunzioni: p0 = pc + pIn·vin + Px·x
        for (size_t r = 0; r < NP; ++r)
        {
            const int bn = bN[r / 2], cn = cN[r / 2];
            auto pick = [&] (const std::array<double, NN>& vec) { return vec[(size_t) bn] - (r % 2 ? vec[(size_t) cn] : 0.0); };
            pc[r] = pick (m0); pIn[r] = pick (mu);
            for (size_t k = 0; k < NS; ++k)
                Px[r][k] = Mx[(size_t) bn][k] - (r % 2 ? Mx[(size_t) cn][k] : 0.0);
        }

        // aggiornamento degli stati: x' = d0 + bu·vin + Ax·x + Ci·i
        for (size_t r = 0; r < NS; ++r)
        {
            // riga r della matrice di aggiornamento (differenza di nodi pesata) applicata a un vettore di nodi
            auto applyRow = [&] (auto&& nodeVal) -> double
            {
                if (r < (size_t) NC)
                {
                    const auto& c = caps[r];
                    const double vi = c.i >= 0 ? nodeVal ((size_t) c.i) : 0.0;
                    const double vj = c.j >= 0 ? nodeVal ((size_t) c.j) : 0.0;
                    return 2.0 * c.geq * (vi - vj);
                }
                return 2.0 * indG * (nodeVal ((size_t) PU) - nodeVal ((size_t) IN));
            };
            const double selfTerm = r < (size_t) NC ? -1.0 : 1.0;            // -J (condensatori), +H (induttanza)
            const double railTerm = (r < (size_t) NC && caps[r].j == RAIL) ? -2.0 * caps[r].geq * vcc : 0.0;
            d0[r] = applyRow ([&] (size_t n) { return m0[n]; }) + railTerm;
            bu[r] = applyRow ([&] (size_t n) { return mu[n]; });
            for (size_t k = 0; k < NS; ++k)
                Ax[r][k] = applyRow ([&] (size_t n) { return Mx[n][k]; }) + (k == r ? selfTerm : 0.0);
            for (size_t k = 0; k < NP; ++k)
                Ci[r][k] = applyRow ([&] (size_t n) { return mi[n][k]; });
        }

        // uscita: cursore del VOLUME
        yc = m0[WV]; yu = mu[WV];
        for (size_t k = 0; k < NS; ++k) yx[k] = Mx[WV][k];
        for (size_t k = 0; k < NP; ++k) yi[k] = mi[WV][k];
    }

    // Esponenziale di giunzione con prolungamento lineare oltre vmax: continuo e con
    // derivata coerente (evita Jacobiani errati e mancate convergenze)
    static inline void junctionExp (double v, double& e, double& de)
    {
        constexpr double vmax = 1.0;
        if (v < -1.0) { e = 0.0; de = 0.0; return; }   // giunzione interdetta: exp(-38) è trascurabile
        if (v <= vmax) { e = std::exp (v / BJT::Vt); de = e / BJT::Vt; }
        else
        {
            const double em = std::exp (vmax / BJT::Vt);
            e = em * (1.0 + (v - vmax) / BJT::Vt);
            de = em / BJT::Vt;
        }
    }

    // Ebers-Moll: correnti e derivate di un transistor (Ib, Ic) rispetto a (Vbe, Vbc)
    static inline void ebersMoll (double vbe, double vbc, double betaF, double& ib, double& ic, double d[2][2])
    {
        using T = BJT;
        double ef, def, er, der;
        junctionExp (vbe, ef, def);
        junctionExp (vbc, er, der);
        ic = T::Is * (ef - er) - T::Is / T::betaR * (er - 1.0);
        ib = T::Is / betaF * (ef - 1.0) + T::Is / T::betaR * (er - 1.0);
        d[0][0] = T::Is / betaF * def;                          // dIb/dVbe
        d[0][1] = T::Is / T::betaR * der;                       // dIb/dVbc
        d[1][0] = T::Is * def;                                  // dIc/dVbe
        d[1][1] = -T::Is * der * (1.0 + 1.0 / T::betaR);        // dIc/dVbc
    }

    // Limitazione del passo di Newton sulle giunzioni (pnjlim, come in SPICE)
    static inline double pnjlim (double vnew, double vold)
    {
        constexpr double Vt = BJT::Vt;
        static const double vcrit = Vt * std::log (Vt / (1.41421356237 * BJT::Is));
        if (vnew > vcrit && std::abs (vnew - vold) > 2.0 * Vt)
        {
            if (vold > 0.0)
            {
                const double arg = 1.0 + (vnew - vold) / Vt;
                vnew = arg > 0.0 ? vold + Vt * std::log (arg) : vcrit;
            }
            else
                vnew = Vt * std::log (std::max (vnew / Vt, 1e-30));
        }
        return vnew;
    }

    // Risolve p = p0 - K·i(p) con Newton-Raphson (passo limitato sulle giunzioni)
    void newton (const std::array<double, NP>& p0, std::array<double, NP>& cur, int maxIter)
    {
        auto& p = pState;
        for (int it = 0; it < maxIter; ++it)
        {
            double d1[2][2], d2[2][2];
            ebersMoll (p[0], p[1], beta1, cur[0], cur[1], d1);
            ebersMoll (p[2], p[3], beta2, cur[2], cur[3], d2);

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
            std::array<double, NP> dp {};
            for (size_t r = 0; r < NP; ++r)
            {
                double vn = p[r] + J[r][NP];
                vn = pnjlim (vn, p[r]);
                vn = std::clamp (vn, -12.0, 1.5);           // fuori da qui non è fisico
                dp[r] = vn - p[r];
                maxStep = std::max (maxStep, std::abs (dp[r]));
                p[r] = vn;
            }
            if (maxStep < 1e-6)
            {
                // convergenza: correnti aggiornate al primo ordine, senza altri esponenziali
                for (size_t c = 0; c < 2; ++c)
                {
                    cur[c]     += d1[c][0] * dp[0] + d1[c][1] * dp[1];
                    cur[c + 2] += d2[c][0] * dp[2] + d2[c][1] * dp[3];
                }
                return;
            }
        }
        double dd[2][2];
        ebersMoll (p[0], p[1], beta1, cur[0], cur[1], dd);
        ebersMoll (p[2], p[3], beta2, cur[2], cur[3], dd);
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
        build (false);                       // condensatori aperti, induttanza in corto
        x.fill (0.0);
        pState = { 0.6, -6.0, 0.6, -1.4 };
        std::array<double, NP> p0 {}, cur {};
        for (size_t r = 0; r < NP; ++r) p0[r] = pc[r];
        newton (p0, cur, 200);
        pPrev = pState;
        std::array<double, NN> vdc {};
        for (size_t r = 0; r < NN; ++r)
        {
            double acc = m0[r];
            for (size_t k = 0; k < NP; ++k) acc += mi[r][k] * cur[k];
            vdc[r] = acc;
        }
        build (true);
        inputCorrection.reset();
        for (size_t k = 0; k < (size_t) NC; ++k) x[k] = caps[k].geq * capVoltage (caps[k], vdc);   // corrente nulla a regime
        x[NC] = indG * (vdc[PU] - vdc[IN]);                                                          // idem per l'induttanza
        lastX = x; lastVin = 0.0; lastCur = cur;
        dcBlock.reset();
    }

    // La scheda audio registra il pick-up quasi "a vuoto" (ingresso Hi-Z da 1 MΩ): il
    // segnale contiene già la risonanza della bobina con la propria capacità, il cavo e
    // i pot della chitarra. Nel FY-2, che carica il pick-up con pochi kΩ, quella risonanza
    // non c'è. Si ricostruisce quindi la forza elettromotrice del pick-up con l'inverso del
    // partitore registrato, limitato sopra i 10 kHz per non esaltare il fruscio della scheda.
    void updateInputCorrection()
    {
        if (pickup.ideal_) { inputCorrection.set (1, 0, 0, 1, 0, 0, fs); inputCorrection.reset(); return; }
        const double G = 1.0 / pickup.Rload + 1.0 / 1.0e6;            // pot + ingresso Hi-Z
        const double Ct = pickup.C + Components::Ccable;
        const double R = pickup.R, L = pickup.L, n0 = 1.0 + R * G;
        const double wp = 2.0 * kPi * 10000.0;
        inputCorrection.set (1.0, (L * G + R * Ct) / n0, L * Ct / n0,
                             1.0, 2.0 / wp, 1.0 / (wp * wp), fs);
    }

    struct OnePole
    {
        double g = 0.0, s = 0.0;
        void reset() { s = 0.0; }
        void setCutoff (double fc, double sr) { const double w = std::tan (kPi * fc / sr); g = w / (1.0 + w); }
        inline double hp (double x) { const double u = (x - s) * g; const double y = u + s; s = y + u; return x - y; }
    };

    double fs = 192000.0, fuzzPos = 0.7, volPos = 0.5, gIn = 0.0, indG = 0.0;
    double vcc = Components::Vcc, beta1 = BJT::betaF, beta2 = BJT::betaF;
    Pickup pickup = Pickup::singleCoil();
    std::array<std::array<double, NN>, NN> Ainv {};
    std::array<std::array<double, NN>, NP> P {};
    std::array<std::array<double, NP>, NN> Q {};
    std::array<std::array<double, NP>, NP> K {};
    std::array<double, NN> bConst {};

    // forma a variabili di stato (vedi build())
    std::array<double, NS> x {}, lastX {}, d0 {}, bu {}, yx {};
    std::array<std::array<double, NS>, NS> Ax {};
    std::array<std::array<double, NP>, NS> Ci {};
    std::array<std::array<double, NS>, NP> Px {};
    std::array<double, NP> pc {}, pIn {}, yi {}, lastCur {};
    std::array<double, NN> m0 {}, mu {};
    std::array<std::array<double, NS>, NN> Mx {};
    std::array<std::array<double, NP>, NN> mi {};
    double yc = 0.0, yu = 0.0, lastVin = 0.0;
    std::array<double, NP> pState { 0.6, -6.0, 0.6, -1.4 }, pPrev { 0.6, -6.0, 0.6, -1.4 };
    OnePole dcBlock;
    AnalogBiquad inputCorrection;
};

} // namespace pf2
