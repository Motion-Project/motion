import { useState } from 'react'
import { FormSection, FormInput } from '@/components/form'

interface UploadSettingsProps {
  config: Record<string, { value: string | number | boolean }>
  onChange: (param: string, value: string | number | boolean) => void
  getError?: (param: string) => string | undefined
}

type UploadProvider = 'rclone' | 's3' | 'gdrive' | 'dropbox' | 'sftp' | 'custom'

const UPLOAD_TEMPLATES: Record<UploadProvider, { label: string; description: string; pictureCmd: string; movieCmd: string }> = {
  rclone: {
    label: 'Rclone',
    description: 'Universal cloud storage sync (supports 40+ providers)',
    pictureCmd: 'rclone copy "%f" remote:motion/pictures/%Y%m%d/',
    movieCmd: 'rclone copy "%f" remote:motion/movies/%Y%m%d/',
  },
  s3: {
    label: 'AWS S3',
    description: 'Amazon S3 or compatible storage (MinIO, DigitalOcean Spaces)',
    pictureCmd: 'aws s3 cp "%f" s3://YOUR_BUCKET/motion/pictures/%Y%m%d/',
    movieCmd: 'aws s3 cp "%f" s3://YOUR_BUCKET/motion/movies/%Y%m%d/',
  },
  gdrive: {
    label: 'Google Drive',
    description: 'Upload via gdrive CLI',
    pictureCmd: 'gdrive files upload --parent YOUR_FOLDER_ID "%f"',
    movieCmd: 'gdrive files upload --parent YOUR_FOLDER_ID "%f"',
  },
  dropbox: {
    label: 'Dropbox',
    description: 'Upload via Dropbox-Uploader script',
    pictureCmd: 'dropbox_uploader.sh upload "%f" /motion/pictures/',
    movieCmd: 'dropbox_uploader.sh upload "%f" /motion/movies/',
  },
  sftp: {
    label: 'SFTP/SCP',
    description: 'Upload to remote server via SSH',
    pictureCmd: 'scp "%f" user@server:/backup/motion/pictures/',
    movieCmd: 'scp "%f" user@server:/backup/motion/movies/',
  },
  custom: {
    label: 'Custom',
    description: 'Enter your own upload command',
    pictureCmd: '',
    movieCmd: '',
  },
}

export function UploadSettings({ config, onChange, getError }: UploadSettingsProps) {
  const [selectedProvider, setSelectedProvider] = useState<UploadProvider>('rclone')

  const getValue = (param: string, defaultValue: string | number | boolean = '') => {
    return config[param]?.value ?? defaultValue
  }

  const currentPictureCmd = String(getValue('on_picture_save', ''))
  const currentMovieCmd = String(getValue('on_movie_end', ''))

  const handleApplyTemplate = () => {
    const template = UPLOAD_TEMPLATES[selectedProvider]
    if (template.pictureCmd) {
      onChange('on_picture_save', template.pictureCmd)
    }
    if (template.movieCmd) {
      onChange('on_movie_end', template.movieCmd)
    }
  }

  return (
    <FormSection
      title="Cloud Upload"
      description="Configure automatic upload of pictures and videos to cloud storage. Uses the same event hooks as notifications."
      collapsible
      defaultOpen={false}
    >
      {/* Info box */}
      <div className="bg-surface rounded-lg p-4 mb-6 text-sm">
        <p className="text-gray-400 mb-2">
          Cloud uploads are triggered by the <code className="bg-surface-elevated px-1 rounded">on_picture_save</code> and <code className="bg-surface-elevated px-1 rounded">on_movie_end</code> event hooks.
          Select a provider template below or enter a custom command.
        </p>
        <p className="text-xs text-gray-500">
          <strong>Note:</strong> You must install the required CLI tools (rclone, aws-cli, etc.) on your Pi before uploads will work.
        </p>
      </div>

      {/* Provider selector */}
      <div className="mb-6 p-4 bg-surface-elevated rounded-lg">
        <h4 className="font-medium mb-3 text-sm">Cloud Provider Templates</h4>
        <div className="flex flex-wrap gap-2 mb-4">
          {(Object.keys(UPLOAD_TEMPLATES) as UploadProvider[]).map((key) => (
            <button
              key={key}
              onClick={() => setSelectedProvider(key)}
              className={`px-3 py-1.5 text-sm rounded transition-colors ${
                selectedProvider === key
                  ? 'bg-primary text-white'
                  : 'bg-surface hover:bg-surface-elevated'
              }`}
            >
              {UPLOAD_TEMPLATES[key].label}
            </button>
          ))}
        </div>

        {selectedProvider !== 'custom' && (
          <div className="text-xs text-gray-400 mb-4">
            <p className="mb-2">{UPLOAD_TEMPLATES[selectedProvider].description}</p>
            <div className="space-y-2">
              <div>
                <span className="text-gray-500">Picture:</span>
                <pre className="mt-1 p-2 bg-surface rounded overflow-x-auto whitespace-pre-wrap break-all">
                  {UPLOAD_TEMPLATES[selectedProvider].pictureCmd}
                </pre>
              </div>
              <div>
                <span className="text-gray-500">Movie:</span>
                <pre className="mt-1 p-2 bg-surface rounded overflow-x-auto whitespace-pre-wrap break-all">
                  {UPLOAD_TEMPLATES[selectedProvider].movieCmd}
                </pre>
              </div>
            </div>
          </div>
        )}

        <button
          onClick={handleApplyTemplate}
          disabled={selectedProvider === 'custom'}
          className="px-4 py-1.5 text-sm bg-primary hover:bg-primary-hover rounded transition-colors disabled:opacity-50"
        >
          Apply Template
        </button>
      </div>

      {/* Current configuration */}
      <FormInput
        label="Picture Upload Command"
        value={currentPictureCmd}
        onChange={(val) => onChange('on_picture_save', val)}
        placeholder="Command to upload pictures"
        helpText="Runs after each picture is saved. %f = filename, %Y%m%d = date"
        error={getError?.('on_picture_save')}
      />

      <FormInput
        label="Movie Upload Command"
        value={currentMovieCmd}
        onChange={(val) => onChange('on_movie_end', val)}
        placeholder="Command to upload videos"
        helpText="Runs after each video recording completes. %f = filename"
        error={getError?.('on_movie_end')}
      />

      {/* Setup guides */}
      <div className="mt-6 p-4 bg-surface rounded-lg">
        <h4 className="font-medium mb-3 text-sm">Quick Setup Guides</h4>
        <div className="space-y-4 text-xs text-gray-400">
          <div>
            <h5 className="font-medium text-gray-300 mb-1">Rclone Setup</h5>
            <pre className="p-2 bg-surface-elevated rounded overflow-x-auto">
{`# Install rclone
sudo apt install rclone

# Configure a remote (interactive)
rclone config

# Test upload
rclone copy /path/to/file remote:folder/`}
            </pre>
          </div>

          <div>
            <h5 className="font-medium text-gray-300 mb-1">AWS S3 Setup</h5>
            <pre className="p-2 bg-surface-elevated rounded overflow-x-auto">
{`# Install AWS CLI
sudo apt install awscli

# Configure credentials
aws configure

# Test upload
aws s3 cp /path/to/file s3://bucket/`}
            </pre>
          </div>
        </div>
      </div>
    </FormSection>
  )
}
