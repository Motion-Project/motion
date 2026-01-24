interface FormToggleProps {
  label: string
  value: boolean
  onChange: (value: boolean) => void
  disabled?: boolean
  helpText?: string
  error?: string
}

export function FormToggle({
  label,
  value,
  onChange,
  disabled = false,
  helpText,
  error,
}: FormToggleProps) {
  const handleChange = () => {
    if (!disabled) {
      onChange(!value)
    }
  }

  const hasError = !!error

  return (
    <div className="mb-4">
      <label className="flex items-center cursor-pointer">
        <div className="relative">
          <input
            type="checkbox"
            checked={value}
            onChange={handleChange}
            disabled={disabled}
            className="sr-only"
            aria-invalid={hasError}
          />
          <div
            className={`block w-14 h-8 rounded-full transition-colors ${
              value ? 'bg-primary' : 'bg-surface-elevated'
            } ${disabled ? 'opacity-50 cursor-not-allowed' : ''} ${
              hasError ? 'ring-2 ring-red-500' : ''
            }`}
          ></div>
          <div
            className={`absolute left-1 top-1 bg-white w-6 h-6 rounded-full transition-transform ${
              value ? 'transform translate-x-6' : ''
            }`}
          ></div>
        </div>
        <div className="ml-3">
          <span className="text-sm font-medium">{label}</span>
          {hasError && (
            <p className="text-sm text-red-400" role="alert">{error}</p>
          )}
          {helpText && !hasError && (
            <p className="text-sm text-gray-400">{helpText}</p>
          )}
        </div>
      </label>
    </div>
  )
}
