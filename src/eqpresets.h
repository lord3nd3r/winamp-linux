#ifndef EQPRESETS_H
#define EQPRESETS_H

// Built-in EQ Presets (slider values 0-63, center=32)
struct EqPreset {
    const char *name;
    int values[10];
};

extern const EqPreset builtinPresets[];
extern const int numPresets;

#endif // EQPRESETS_H
