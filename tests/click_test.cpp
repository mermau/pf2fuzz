// Verifica anti-click: note pizzicate di ampiezza crescente, l'uscita non deve avere salti
// anomali (sintomo di mancata convergenza del risolutore). Esce con 1 se ne trova.
#include "FuzzCircuit.h"
#include <cstdio>
#include <random>
using namespace pf2;
int main()
{
    const double fs = 192000; bool bad = false;
    for (double peak : { 0.05, 0.2, 0.5, 1.0 })
        for (double fz : { 0.0, 0.7, 1.0 })
        {
            std::mt19937 rng (1);
            FuzzCircuit c; c.setPickup (Pickup::humbucker()); c.prepare (fs); c.setControls ((float) fz, 0.8f);
            double prev = 0, prev2 = 0, maxJump = 0, t0 = 0, f = 110;
            for (int i = 0; i < (int) (fs * 4); ++i)
            {
                const double t = i / fs;
                if (t - t0 > 0.5) { t0 = t; f = 82.4 * std::pow (2.0, (rng() % 24) / 12.0); }
                const double tt = t - t0, env = peak * std::exp (-tt * 3.0) * (1 - std::exp (-tt * 2000));
                const double y = c.process (env * (std::sin (2 * kPi * f * tt) + 0.5 * std::sin (4 * kPi * f * tt)));
                if (! std::isfinite (y)) bad = true;
                maxJump = std::max (maxJump, std::abs (y - 2 * prev + prev2));
                prev2 = prev; prev = y;
            }
            const bool ok = maxJump < 0.2;
            bad |= ! ok;
            std::printf ("picco %.2f V, fuzz %.1f: salto massimo %.4f %s\n", peak, fz, maxJump, ok ? "OK" : "CLICK");
        }

    // casi estremi del pannello interno: hFE ai limiti e batteria che si scarica suonando
    for (auto [h1, h2] : { std::pair<double, double> { 60, 60 }, { 560, 560 }, { 60, 560 }, { 560, 60 } })
    {
        std::mt19937 rng (3);
        FuzzCircuit c; c.setPickup (Pickup::humbucker()); c.setHfe (h1, h2); c.setSupply (9.6); c.prepare (fs); c.setControls (1.0f, 0.8f);
        double prev = 0, prev2 = 0, maxJump = 0, t0 = 0, f = 110;
        for (int i = 0; i < (int) (fs * 4); ++i)
        {
            const double t = i / fs;
            if (i % 128 == 0) c.setSupply (9.6 - 5.1 * std::min (1.0, t / 3.5));
            if (t - t0 > 0.4) { t0 = t; f = 82.4 * std::pow (2.0, (rng() % 24) / 12.0); }
            const double tt = t - t0, env = 0.8 * std::exp (-tt * 3.0) * (1 - std::exp (-tt * 2000));
            const double y = c.process (env * (std::sin (2 * kPi * f * tt) + 0.5 * std::sin (4 * kPi * f * tt)));
            if (! std::isfinite (y)) bad = true;
            maxJump = std::max (maxJump, std::abs (y - 2 * prev + prev2));
            prev2 = prev; prev = y;
        }
        const bool ok = maxJump < 0.2;
        bad |= ! ok;
        std::printf ("hFE %3.0f/%3.0f, batteria 9.6 -> 4.5 V: salto massimo %.4f %s\n", h1, h2, maxJump, ok ? "OK" : "CLICK");
    }
    return bad ? 1 : 0;
}
