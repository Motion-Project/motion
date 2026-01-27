import { useState } from 'react';
import { FormSection, FormSelect, FormToggle, FormSlider } from '@/components/form';

interface Preferences {
  gridColumns: number;
  gridRows: number;
  fitFramesVertically: boolean;
  playbackFramerateFactor: number;
  playbackResolutionFactor: number;
  theme: 'dark' | 'light' | 'auto';
}

const DEFAULT_PREFERENCES: Preferences = {
  gridColumns: 2,
  gridRows: 2,
  fitFramesVertically: false,
  playbackFramerateFactor: 1.0,
  playbackResolutionFactor: 1.0,
  theme: 'dark',
};

function loadPreferences(): Preferences {
  const stored = localStorage.getItem('motion-ui-preferences');
  if (stored) {
    try {
      const parsed = JSON.parse(stored);
      return { ...DEFAULT_PREFERENCES, ...parsed };
    } catch (e) {
      console.error('Failed to parse preferences:', e);
    }
  }
  return DEFAULT_PREFERENCES;
}

export function PreferencesSettings() {
  const [preferences, setPreferences] = useState<Preferences>(loadPreferences);

  // Save preferences to localStorage when they change
  const updatePreference = <K extends keyof Preferences>(
    key: K,
    value: Preferences[K]
  ) => {
    const updated = { ...preferences, [key]: value };
    setPreferences(updated);
    localStorage.setItem('motion-ui-preferences', JSON.stringify(updated));
  };

  return (
    <FormSection
      title="UI Preferences"
      description="User interface preferences (stored locally in browser)"
      collapsible
      defaultOpen={false}
    >
      <div className="space-y-4">
        <div className="text-xs text-gray-400 bg-surface-elevated p-3 rounded mb-4">
          <strong>Note:</strong> These preferences are stored in your browser's localStorage and
          are not saved to the Motion server. They are specific to this browser/device.
        </div>

        <div className="border-t border-surface-elevated pt-4">
          <h4 className="font-medium mb-3">Dashboard Layout</h4>

          <FormSlider
            label="Grid Columns"
            value={preferences.gridColumns}
            onChange={(val) => updatePreference('gridColumns', val)}
            min={1}
            max={4}
            helpText="Number of camera columns in dashboard grid (1-4)"
          />

          <FormSlider
            label="Grid Rows"
            value={preferences.gridRows}
            onChange={(val) => updatePreference('gridRows', val)}
            min={1}
            max={4}
            helpText="Number of camera rows in dashboard grid (1-4)"
          />

          <FormToggle
            label="Fit Frames Vertically"
            value={preferences.fitFramesVertically}
            onChange={(val) => updatePreference('fitFramesVertically', val)}
            helpText="Fit camera frames to viewport height instead of width"
          />
        </div>

        <div className="border-t border-surface-elevated pt-4">
          <h4 className="font-medium mb-3">Playback Settings</h4>

          <FormSlider
            label="Framerate Factor"
            value={preferences.playbackFramerateFactor}
            onChange={(val) => updatePreference('playbackFramerateFactor', val)}
            min={0.1}
            max={4.0}
            step={0.1}
            unit="x"
            helpText="Playback speed multiplier (0.5 = half speed, 2.0 = double speed)"
          />

          <FormSlider
            label="Resolution Factor"
            value={preferences.playbackResolutionFactor}
            onChange={(val) => updatePreference('playbackResolutionFactor', val)}
            min={0.25}
            max={1.0}
            step={0.25}
            unit="x"
            helpText="Playback resolution scaling (0.5 = half resolution, 1.0 = full)"
          />

          <div className="text-xs text-gray-400">
            <p>Lower resolution factors reduce bandwidth and improve performance on slow connections.</p>
          </div>
        </div>

        <div className="border-t border-surface-elevated pt-4">
          <h4 className="font-medium mb-3">Appearance</h4>

          {/* TODO: Enable when theme feature is implemented */}
          <FormSelect
            label="Theme"
            value={preferences.theme}
            onChange={(val) => updatePreference('theme', val as 'dark' | 'light' | 'auto')}
            options={[
              { value: 'dark', label: 'Dark' },
              { value: 'light', label: 'Light (Coming Soon)' },
              { value: 'auto', label: 'Auto (System Preference)' },
            ]}
            helpText="UI color theme"
            disabled={true}
          />

          <div className="text-xs text-yellow-200 bg-yellow-600/10 border border-yellow-600/30 p-3 rounded">
            <strong>Note:</strong> Light theme and auto theme switching will be available in a future update.
          </div>
        </div>

        <div className="border-t border-surface-elevated pt-4">
          <button
            onClick={() => {
              localStorage.removeItem('motion-ui-preferences');
              setPreferences(DEFAULT_PREFERENCES);
            }}
            className="px-4 py-2 bg-red-600 hover:bg-red-700 rounded-lg transition-colors text-sm"
          >
            Reset to Defaults
          </button>
          <p className="text-xs text-gray-400 mt-2">
            Clears all saved preferences and returns to default settings
          </p>
        </div>
      </div>
    </FormSection>
  );
}
