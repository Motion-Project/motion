import { type ChangeEvent } from 'react'

interface FormSelectOption {
  value: string
  label: string
}

interface FormSelectProps {
  label: string
  value: string
  onChange: (value: string) => void
  options: FormSelectOption[]
  disabled?: boolean
  required?: boolean
  helpText?: string
  error?: string
}

export function FormSelect({
  label,
  value,
  onChange,
  options,
  disabled = false,
  required = false,
  helpText,
  error,
}: FormSelectProps) {
  const handleChange = (e: ChangeEvent<HTMLSelectElement>) => {
    onChange(e.target.value)
  }

  const hasError = !!error

  return (
    <div className="mb-4">
      <label className="block text-sm font-medium mb-1">
        {label}
        {required && <span className="text-red-500 ml-1">*</span>}
      </label>
      <select
        value={value}
        onChange={handleChange}
        disabled={disabled}
        required={required}
        className={`w-full px-3 py-2 bg-surface border rounded-lg focus:outline-none focus:ring-2 disabled:opacity-50 disabled:cursor-not-allowed ${
          hasError
            ? 'border-red-500 focus:ring-red-500'
            : 'border-surface-elevated focus:ring-primary'
        }`}
        aria-invalid={hasError}
        aria-describedby={hasError ? `${label}-error` : undefined}
      >
        {options.map((option) => (
          <option key={option.value} value={option.value}>
            {option.label}
          </option>
        ))}
      </select>
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
