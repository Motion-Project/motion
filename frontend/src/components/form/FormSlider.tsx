import { type ChangeEvent } from 'react'

interface FormSliderProps {
  label: string
  value: number
  onChange: (value: number) => void
  min: number
  max: number
  step?: number
  unit?: string
  disabled?: boolean
  required?: boolean
  helpText?: string
  error?: string
  showValue?: boolean
  scale?: 'linear' | 'logarithmic'
}

export function FormSlider({
  label,
  value,
  onChange,
  min,
  max,
  step = 1,
  unit = '',
  disabled = false,
  required = false,
  helpText,
  error,
  showValue = true,
  scale = 'linear',
}: FormSliderProps) {
  // Logarithmic scale conversion functions
  const valueToSlider = (val: number): number => {
    if (scale === 'linear') return val

    // Handle special case: 0 = auto (maps to slider position 0)
    if (val === 0) return min

    // Use logarithmic scaling for values > 0
    // Handle min=0 case by using 1 as the effective minimum for log calculation
    const effectiveMin = min === 0 ? 1 : min
    const logMin = Math.log(effectiveMin)
    const logMax = Math.log(max)
    const logVal = Math.log(val)

    return effectiveMin + ((logVal - logMin) / (logMax - logMin)) * (max - effectiveMin)
  }

  const sliderToValue = (sliderVal: number): number => {
    if (scale === 'linear') return sliderVal

    // Handle special case: slider at min = 0 (auto)
    if (sliderVal === min) return 0

    // Convert from linear slider position to logarithmic value
    const effectiveMin = min === 0 ? 1 : min
    const logMin = Math.log(effectiveMin)
    const logMax = Math.log(max)

    const position = (sliderVal - effectiveMin) / (max - effectiveMin)
    const logVal = logMin + position * (logMax - logMin)

    return Math.round(Math.exp(logVal))
  }

  const handleChange = (e: ChangeEvent<HTMLInputElement>) => {
    const sliderValue = Number(e.target.value)
    const actualValue = sliderToValue(sliderValue)
    onChange(actualValue)
  }

  const hasError = !!error

  // Calculate percentage for gradient fill
  const sliderValue = valueToSlider(value)
  const percentage = ((sliderValue - min) / (max - min)) * 100

  // Calculate middle value for labels (considering scale type)
  const middleValue = scale === 'logarithmic'
    ? sliderToValue(min + (max - min) / 2)
    : (min + max) / 2

  // Format value for display
  const formatValue = (val: number): string => {
    if (val === 0 && min === 0) return '0'
    if (Number.isInteger(val)) return val.toString()
    return val.toFixed(step >= 1 ? 0 : step.toString().split('.')[1]?.length || 1)
  }

  return (
    <div className="mb-4">
      <div className="flex justify-between items-center mb-1">
        <label className="block text-sm font-medium">
          {label}
          {required && <span className="text-red-500 ml-1">*</span>}
        </label>
        {showValue && (
          <span className="text-sm font-mono text-gray-400">
            {value}{unit}
          </span>
        )}
      </div>

      <div className="relative">
        <input
          type="range"
          value={sliderValue}
          onChange={handleChange}
          min={min}
          max={max}
          step={scale === 'logarithmic' ? 1 : step}
          disabled={disabled}
          className={`w-full h-2 rounded-lg appearance-none cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed
            ${hasError ? 'slider-error' : 'slider-default'}`}
          style={{
            background: `linear-gradient(to right,
              rgb(59, 130, 246) 0%,
              rgb(59, 130, 246) ${percentage}%,
              rgb(55, 65, 81) ${percentage}%,
              rgb(55, 65, 81) 100%)`
          }}
          aria-invalid={hasError}
          aria-describedby={hasError ? `${label}-error` : undefined}
        />

        {/* Tick marks */}
        <div className="relative w-full h-4 mt-1">
          {[0, 25, 50, 75, 100].map((position) => {
            const isMajor = position === 0 || position === 50 || position === 100
            return (
              <div
                key={position}
                className="absolute"
                style={{ left: `${position}%`, transform: 'translateX(-50%)' }}
              >
                <div
                  className={`w-px bg-gray-500 mx-auto ${
                    isMajor ? 'h-3' : 'h-2'
                  }`}
                />
              </div>
            )
          })}
        </div>

        {/* Tick labels */}
        <div className="relative w-full">
          <div className="flex justify-between text-xs text-gray-500 px-1">
            <span>{formatValue(min)}{unit}</span>
            <span>{formatValue(middleValue)}{unit}</span>
            <span>{formatValue(max)}{unit}</span>
          </div>
        </div>
      </div>

      {hasError && (
        <p id={`${label}-error`} className="mt-1 text-sm text-red-400" role="alert">
          {error}
        </p>
      )}
      {helpText && !hasError && (
        <p className="mt-1 text-sm text-gray-400">{helpText}</p>
      )}

      <style>{`
        input[type='range']::-webkit-slider-thumb {
          appearance: none;
          width: 16px;
          height: 16px;
          border-radius: 50%;
          background: rgb(59, 130, 246);
          cursor: pointer;
          border: 2px solid rgb(30, 41, 59);
        }

        input[type='range']::-webkit-slider-thumb:hover {
          background: rgb(96, 165, 250);
        }

        input[type='range']::-moz-range-thumb {
          width: 16px;
          height: 16px;
          border-radius: 50%;
          background: rgb(59, 130, 246);
          cursor: pointer;
          border: 2px solid rgb(30, 41, 59);
        }

        input[type='range']::-moz-range-thumb:hover {
          background: rgb(96, 165, 250);
        }

        input[type='range'].slider-error::-webkit-slider-thumb {
          background: rgb(239, 68, 68);
        }

        input[type='range'].slider-error::-moz-range-thumb {
          background: rgb(239, 68, 68);
        }
      `}</style>
    </div>
  )
}
