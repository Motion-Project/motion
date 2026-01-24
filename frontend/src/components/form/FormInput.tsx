import { type ChangeEvent, useState } from 'react'

interface FormInputProps {
  label: string
  value: string | number
  onChange: (value: string) => void
  type?: 'text' | 'number' | 'password'
  placeholder?: string
  disabled?: boolean
  required?: boolean
  helpText?: string
  error?: string
  min?: string | number
  max?: string | number
  step?: string | number
  /** Original value from server - used to show modified indicator */
  originalValue?: string | number
  /** Show visibility toggle for password fields (default: true for password type) */
  showVisibilityToggle?: boolean
}

export function FormInput({
  label,
  value,
  onChange,
  type = 'text',
  placeholder,
  disabled = false,
  required = false,
  helpText,
  error,
  min,
  max,
  step,
  originalValue,
  showVisibilityToggle,
}: FormInputProps) {
  const [showPassword, setShowPassword] = useState(false)

  const handleChange = (e: ChangeEvent<HTMLInputElement>) => {
    onChange(e.target.value)
  }

  const hasError = !!error

  // Determine if field is modified (comparing string representations)
  const isModified = originalValue !== undefined && String(value) !== String(originalValue)

  // Determine actual input type (handle password visibility toggle)
  const isPasswordType = type === 'password'
  const shouldShowToggle = showVisibilityToggle ?? isPasswordType
  const actualType = isPasswordType && showPassword ? 'text' : type

  return (
    <div className="mb-4">
      <label className="block text-sm font-medium mb-1">
        {label}
        {required && <span className="text-red-500 ml-1">*</span>}
        {isModified && (
          <span className="ml-2 text-xs text-yellow-400">(modified)</span>
        )}
      </label>
      <div className="relative">
        <input
          type={actualType}
          value={value}
          onChange={handleChange}
          placeholder={placeholder}
          disabled={disabled}
          required={required}
          min={min}
          max={max}
          step={step}
          className={`w-full px-3 py-2 bg-surface border rounded-lg focus:outline-none focus:ring-2 disabled:opacity-50 disabled:cursor-not-allowed ${
            shouldShowToggle ? 'pr-10' : ''
          } ${
            hasError
              ? 'border-red-500 focus:ring-red-500'
              : isModified
                ? 'border-yellow-500/50 focus:ring-yellow-500'
                : 'border-surface-elevated focus:ring-primary'
          }`}
          aria-invalid={hasError}
          aria-describedby={hasError ? `${label}-error` : undefined}
        />
        {shouldShowToggle && (
          <button
            type="button"
            onClick={() => setShowPassword(!showPassword)}
            className="absolute right-2 top-1/2 -translate-y-1/2 p-1 text-gray-400 hover:text-gray-200 transition-colors"
            aria-label={showPassword ? 'Hide password' : 'Show password'}
          >
            {showPassword ? (
              <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M13.875 18.825A10.05 10.05 0 0112 19c-4.478 0-8.268-2.943-9.543-7a9.97 9.97 0 011.563-3.029m5.858.908a3 3 0 114.243 4.243M9.878 9.878l4.242 4.242M9.88 9.88l-3.29-3.29m7.532 7.532l3.29 3.29M3 3l3.59 3.59m0 0A9.953 9.953 0 0112 5c4.478 0 8.268 2.943 9.543 7a10.025 10.025 0 01-4.132 5.411m0 0L21 21" />
              </svg>
            ) : (
              <svg className="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" />
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z" />
              </svg>
            )}
          </button>
        )}
      </div>
      {hasError && (
        <p id={`${label}-error`} className="mt-1 text-sm text-red-400" role="alert">
          {error}
        </p>
      )}
      {helpText && !hasError && (
        <p className="mt-1 text-sm text-gray-400">{helpText}</p>
      )}
    </div>
  )
}
