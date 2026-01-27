import { FormSection, FormToggle } from '@/components/form';
import { SchedulePicker } from '@/components/schedule';

export interface ScheduleSettingsProps {
  config: Record<string, { value: string | number | boolean }>;
  onChange: (param: string, value: string | number | boolean) => void;
  getError?: (param: string) => string | undefined;
}

export function ScheduleSettings({ config, onChange, getError }: ScheduleSettingsProps) {
  const getValue = (param: string, defaultValue: string | number | boolean = '') => {
    return config[param]?.value ?? defaultValue;
  };

  // Motion Detection Schedule
  const scheduleParams = String(getValue('schedule_params', ''));
  const isMotionScheduleEnabled = scheduleParams.trim() !== '';

  const handleMotionScheduleToggle = (enabled: boolean) => {
    if (!enabled) {
      onChange('schedule_params', '');
    } else {
      // Default: pause motion detection Mon-Fri 9am-5pm
      onChange('schedule_params', 'default=true action=pause mon-fri=0900-1700');
    }
  };

  // Continuous Recording Schedule
  const pictureScheduleParams = String(getValue('picture_schedule_params', ''));
  const isPictureScheduleEnabled = pictureScheduleParams.trim() !== '';

  const handlePictureScheduleToggle = (enabled: boolean) => {
    if (!enabled) {
      onChange('picture_schedule_params', '');
    } else {
      // Default: enable continuous recording Mon-Fri 9am-5pm
      onChange('picture_schedule_params', 'default=false action=pause mon-fri=0900-1700');
    }
  };

  return (
    <FormSection
      title="Schedules"
      description="Configure when motion detection and continuous recording are active"
      collapsible
      defaultOpen={false}
    >
      <div className="space-y-6">
        {/* Motion Detection Schedule */}
        <div className="space-y-4">
          <h4 className="text-sm font-medium border-b border-gray-700 pb-2">
            Motion Detection Schedule
          </h4>
          <p className="text-xs text-gray-400">
            Control when motion detection is active or paused
          </p>
          <FormToggle
            label="Enable Motion Detection Schedule"
            value={isMotionScheduleEnabled}
            onChange={handleMotionScheduleToggle}
            helpText="When enabled, motion detection follows the schedule below"
          />

          {isMotionScheduleEnabled && (
            <SchedulePicker
              value={scheduleParams}
              onChange={(val) => onChange('schedule_params', val)}
              error={getError?.('schedule_params')}
            />
          )}
        </div>

        {/* Divider */}
        <div className="border-t border-gray-700" />

        {/* Continuous Recording Schedule */}
        <div className="space-y-4">
          <h4 className="text-sm font-medium border-b border-gray-700 pb-2">
            Continuous Recording Schedule
          </h4>
          <p className="text-xs text-gray-400">
            Control when continuous picture capture (timelapse) is active
          </p>
          <FormToggle
            label="Enable Continuous Recording Schedule"
            value={isPictureScheduleEnabled}
            onChange={handlePictureScheduleToggle}
            helpText="When enabled, continuous recording follows the schedule below"
          />

          {isPictureScheduleEnabled && (
            <SchedulePicker
              value={pictureScheduleParams}
              onChange={(val) => onChange('picture_schedule_params', val)}
              error={getError?.('picture_schedule_params')}
            />
          )}
        </div>
      </div>
    </FormSection>
  );
}
