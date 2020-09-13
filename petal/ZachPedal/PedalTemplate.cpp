#include "daisy_petal.h"
#include "daisysp.h" 

#include <vector>

using namespace daisy;
using namespace daisysp;

// Defines for Interleaved Audio
#define LEFT (i)
#define RIGHT (i + 1)

DaisyPetal hw;

class Effect_t
{
public:
    virtual void init(float samplerate) = 0;
    virtual void selected() = 0;
    virtual void deselected() = 0;
    virtual void pre_process() = 0;
    virtual void process(float& left, float& right) = 0;
    virtual void post_process() = 0;
    
    bool active = false;
    bool sw1_active = false;
    bool sw2_active = false;
};

class BitCrusher : public Effect_t
{
public:
    void init(float samplerate) override
    {

    }

    void selected() override
    {

    }

    void deselected() override
    {

    }

    void pre_process() override
    {
        
    }

    void process(float& left, float& right) override
    {
        left = (left > bias) ? copysign(1.0, left) : 0.0f;
        right = (right > bias) ? copysign(1.0, right) : 0.0f;
    }

    void post_process() override
    {
        
    }

    float bias = 0.01f;
};

class Reverb : public Effect_t
{
public:
    void init(float samplerate) override
    {
        verb.Init(samplerate);
    }

    void selected() override
    {
        vtime.Init(hw.knob[hw.KNOB_1], 0.6f, 0.999f, Parameter::LOGARITHMIC);
        vfreq.Init(hw.knob[hw.KNOB_2], 500.0f, 20000.0f, Parameter::LOGARITHMIC);
        vsend.Init(hw.knob[hw.KNOB_3], 0.0f, 1.0f, Parameter::LINEAR);
    }

    void deselected() override
    {

    }

    void pre_process() override
    {
        verb.SetFeedback(vtime.Process());
        verb.SetLpFreq(vfreq.Process());
        vsend.Process();
    }

    void process(float& left, float& right) override
    {
        dryl = left;
        dryr = right;

        sendl = dryl * vsend.Value();
        sendr = dryr * vsend.Value();
        verb.Process(sendl, sendr, &wetl, &wetr);

        left = dryl + wetl;
        right = dryr + wetr;
    }

    void post_process() override
    {
        
    }

    Parameter vtime, vfreq, vsend;
    ReverbSc verb;
    float dryl, dryr, wetl, wetr, sendl, sendr;
};

std::vector<Effect_t*> effects;
uint8_t selected = 0;
bool first_boot = true;

void AudioCallback(float *in, float *out, size_t size)
{
    hw.DebounceControls();
    hw.UpdateAnalogControls();

    float drywet = hw.knob[0].Process();

    bool effect_changed = false;
    if(hw.switches[DaisyPetal::SW_4].RisingEdge())
    {
        ++selected;
        if (selected >= effects.size())
        {
            selected = 0;
        }
        effect_changed = true;
    }

    Effect_t* selected_effect = effects[selected];

    if (effect_changed || first_boot)
    {   first_boot = false;
        selected_effect->selected();
    }

    if(hw.switches[DaisyPetal::SW_1].RisingEdge())
    {
        selected_effect->sw1_active = !selected_effect->sw1_active;
    }
    
    if(hw.switches[DaisyPetal::SW_2].RisingEdge())
    {
        selected_effect->sw2_active = !selected_effect->sw2_active;
    }

    if(hw.switches[DaisyPetal::SW_3].RisingEdge())
    {
        selected_effect->active = !selected_effect->active;
    }

    for (auto effect : effects)
    {
        effect->pre_process();
    }

    for (size_t i = 0; i < size; i+=2)
    {
        float inputL = in[LEFT];
        float inputR = in[RIGHT];

        for (auto effect : effects)
        {
            if (effect->active)
            {
                effect->process(inputL, inputR);
            }
        }

        out[LEFT] = inputL * drywet + in[LEFT] * (1 - drywet);
        out[RIGHT] = inputR * drywet + in[RIGHT] * (1 - drywet);
    }

    for (auto effect : effects)
    {
        effect->post_process();
    }
}

int main(void)
{
    hw.Init();

    float samplerate = hw.AudioSampleRate();

    effects.push_back(new BitCrusher());
    effects.push_back(new Reverb());

    for (auto effect : effects)
    {
        effect->init(samplerate);
    }

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while (1)
    {
        hw.ClearLeds();

        // Draw
        hw.SetFootswitchLed(hw.FOOTSWITCH_LED_1, effects[selected]->sw1_active ? 1.0f : 0.0f);
        hw.SetFootswitchLed(hw.FOOTSWITCH_LED_2, effects[selected]->sw2_active ? 1.0f : 0.0f);
        hw.SetFootswitchLed(hw.FOOTSWITCH_LED_3, effects[selected]->active ? 1.0f : 0.0f);
        hw.SetRingLed(static_cast<DaisyPetal::RingLed>(selected), 1.0f, 1.0f, 1.0f);

        hw.UpdateLeds();

        dsy_system_delay(16); // 60Hz
    }
}
