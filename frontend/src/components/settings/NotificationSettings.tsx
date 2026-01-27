import { useState } from 'react'
import { FormSection, FormInput } from '@/components/form'

interface NotificationSettingsProps {
  config: Record<string, { value: string | number | boolean }>
  onChange: (param: string, value: string | number | boolean) => void
  getError?: (param: string) => string | undefined
}

type NotificationTemplate = 'custom' | 'webhook' | 'telegram' | 'email' | 'pushover'

const TEMPLATES: Record<NotificationTemplate, { label: string; description: string; command: string }> = {
  custom: {
    label: 'Custom Command',
    description: 'Enter your own shell command',
    command: '',
  },
  webhook: {
    label: 'Webhook (HTTP POST)',
    description: 'Send HTTP POST to a URL',
    command: 'curl -s -X POST -H "Content-Type: application/json" -d \'{"camera":"%t","event":"%v","time":"%Y-%m-%d %T"}\' "YOUR_WEBHOOK_URL"',
  },
  telegram: {
    label: 'Telegram Bot',
    description: 'Send message via Telegram Bot API',
    command: 'curl -s -X POST "https://api.telegram.org/botYOUR_BOT_TOKEN/sendMessage" -d "chat_id=YOUR_CHAT_ID&text=Motion detected on %t at %Y-%m-%d %T"',
  },
  email: {
    label: 'Email (msmtp)',
    description: 'Send email using msmtp',
    command: 'echo -e "Subject: Motion Alert\\n\\nMotion detected on camera %t at %Y-%m-%d %T" | msmtp recipient@example.com',
  },
  pushover: {
    label: 'Pushover',
    description: 'Send push notification via Pushover',
    command: 'curl -s -F "token=YOUR_APP_TOKEN" -F "user=YOUR_USER_KEY" -F "message=Motion on %t at %T" https://api.pushover.net/1/messages.json',
  },
}

export function NotificationSettings({ config, onChange, getError }: NotificationSettingsProps) {
  const [selectedTemplate, setSelectedTemplate] = useState<NotificationTemplate>('custom')

  const getValue = (param: string, defaultValue: string | number | boolean = '') => {
    return config[param]?.value ?? defaultValue
  }

  const handleTemplateApply = (eventType: 'on_event_start' | 'on_picture_save' | 'on_movie_end') => {
    const template = TEMPLATES[selectedTemplate]
    if (template.command) {
      onChange(eventType, template.command)
    }
  }

  return (
    <FormSection
      title="Notifications & Scripts"
      description="Configure commands that run on motion events. Use script hooks to send notifications via webhooks, Telegram, email, or custom scripts."
      collapsible
      defaultOpen={false}
    >
      {/* Info box about substitution variables */}
      <div className="bg-surface rounded-lg p-4 mb-6 text-sm">
        <h4 className="font-medium mb-2">Available Variables</h4>
        <div className="grid grid-cols-2 md:grid-cols-3 gap-2 text-xs text-gray-400">
          <code>%f</code> <span>Filename</span>
          <code>%t</code> <span>Camera name</span>
          <code>%v</code> <span>Event number</span>
          <code>%Y</code> <span>Year (4 digit)</span>
          <code>%m</code> <span>Month (01-12)</span>
          <code>%d</code> <span>Day (01-31)</span>
          <code>%H</code> <span>Hour (00-23)</span>
          <code>%M</code> <span>Minute (00-59)</span>
          <code>%S</code> <span>Second (00-59)</span>
          <code>%T</code> <span>Time HH:MM:SS</span>
        </div>
      </div>

      {/* Template selector */}
      <div className="mb-6 p-4 bg-surface-elevated rounded-lg">
        <h4 className="font-medium mb-3 text-sm">Quick Setup Templates</h4>
        <div className="flex flex-wrap gap-2 mb-3">
          {(Object.keys(TEMPLATES) as NotificationTemplate[]).map((key) => (
            <button
              key={key}
              onClick={() => setSelectedTemplate(key)}
              className={`px-3 py-1.5 text-sm rounded transition-colors ${
                selectedTemplate === key
                  ? 'bg-primary text-white'
                  : 'bg-surface hover:bg-surface-elevated'
              }`}
            >
              {TEMPLATES[key].label}
            </button>
          ))}
        </div>
        {selectedTemplate !== 'custom' && (
          <div className="text-xs text-gray-400 mb-3">
            <p>{TEMPLATES[selectedTemplate].description}</p>
            <pre className="mt-2 p-2 bg-surface rounded text-xs overflow-x-auto whitespace-pre-wrap break-all">
              {TEMPLATES[selectedTemplate].command}
            </pre>
          </div>
        )}
        <div className="flex gap-2">
          <button
            onClick={() => handleTemplateApply('on_event_start')}
            disabled={selectedTemplate === 'custom'}
            className="px-3 py-1.5 text-xs bg-surface hover:bg-surface-elevated rounded transition-colors disabled:opacity-50"
          >
            Apply to Event Start
          </button>
          <button
            onClick={() => handleTemplateApply('on_picture_save')}
            disabled={selectedTemplate === 'custom'}
            className="px-3 py-1.5 text-xs bg-surface hover:bg-surface-elevated rounded transition-colors disabled:opacity-50"
          >
            Apply to Picture Save
          </button>
          <button
            onClick={() => handleTemplateApply('on_movie_end')}
            disabled={selectedTemplate === 'custom'}
            className="px-3 py-1.5 text-xs bg-surface hover:bg-surface-elevated rounded transition-colors disabled:opacity-50"
          >
            Apply to Movie End
          </button>
        </div>
      </div>

      {/* Event hooks */}
      <FormInput
        label="On Event Start"
        value={String(getValue('on_event_start', ''))}
        onChange={(val) => onChange('on_event_start', val)}
        placeholder="Command to run when motion event starts"
        helpText="Executed when a new motion event begins"
        error={getError?.('on_event_start')}
      />

      <FormInput
        label="On Event End"
        value={String(getValue('on_event_end', ''))}
        onChange={(val) => onChange('on_event_end', val)}
        placeholder="Command to run when motion event ends"
        helpText="Executed when motion event ends (after gap timeout)"
        error={getError?.('on_event_end')}
      />

      <FormInput
        label="On Motion Detected"
        value={String(getValue('on_motion_detected', ''))}
        onChange={(val) => onChange('on_motion_detected', val)}
        placeholder="Command to run on each motion frame"
        helpText="Executed on every frame with motion (can be frequent!)"
        error={getError?.('on_motion_detected')}
      />

      <FormInput
        label="On Picture Save"
        value={String(getValue('on_picture_save', ''))}
        onChange={(val) => onChange('on_picture_save', val)}
        placeholder="Command to run when picture is saved"
        helpText="Executed after a snapshot is saved (%f = filename)"
        error={getError?.('on_picture_save')}
      />

      <FormInput
        label="On Movie Start"
        value={String(getValue('on_movie_start', ''))}
        onChange={(val) => onChange('on_movie_start', val)}
        placeholder="Command to run when recording starts"
        helpText="Executed when video recording begins"
        error={getError?.('on_movie_start')}
      />

      <FormInput
        label="On Movie End"
        value={String(getValue('on_movie_end', ''))}
        onChange={(val) => onChange('on_movie_end', val)}
        placeholder="Command to run when recording ends"
        helpText="Executed when video recording is complete (%f = filename)"
        error={getError?.('on_movie_end')}
      />

      {/* Example notification script */}
      <div className="mt-6 p-4 bg-surface rounded-lg">
        <h4 className="font-medium mb-2 text-sm">Example: Send Picture via Telegram</h4>
        <pre className="text-xs text-gray-400 overflow-x-auto whitespace-pre-wrap">
{`# On Picture Save:
curl -F chat_id="YOUR_CHAT_ID" \\
     -F photo=@"%f" \\
     -F caption="Motion on %t at %T" \\
     "https://api.telegram.org/botYOUR_TOKEN/sendPhoto"`}
        </pre>
      </div>
    </FormSection>
  )
}
