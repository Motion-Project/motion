import { useEffect, useCallback, useRef, type ReactNode } from 'react'
import { useSheetGestures } from '@/hooks/useSheetGestures'

interface BottomSheetProps {
  isOpen: boolean
  onClose: () => void
  title: string
  children: ReactNode
  headerRight?: ReactNode
}

export function BottomSheet({
  isOpen,
  onClose,
  title,
  children,
  headerRight,
}: BottomSheetProps) {
  const sheetRef = useRef<HTMLDivElement>(null)
  const contentRef = useRef<HTMLDivElement>(null)
  const previousFocusRef = useRef<HTMLElement | null>(null)

  const { handlers, style } = useSheetGestures({
    isOpen,
    onClose,
    sheetRef,
  })

  // Handle escape key and focus trap
  useEffect(() => {
    if (!isOpen) return

    const handleEscape = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        onClose()
      }
    }

    const handleTab = (e: KeyboardEvent) => {
      if (e.key !== 'Tab' || !sheetRef.current) return

      // Get all focusable elements within the sheet
      const focusableElements = sheetRef.current.querySelectorAll<HTMLElement>(
        'button:not([disabled]), [href], input:not([disabled]), select:not([disabled]), textarea:not([disabled]), [tabindex]:not([tabindex="-1"])'
      )

      const firstElement = focusableElements[0]
      const lastElement = focusableElements[focusableElements.length - 1]

      if (!firstElement) return

      // Trap focus within modal
      if (e.shiftKey) {
        // Shift+Tab: moving backwards
        if (document.activeElement === firstElement) {
          e.preventDefault()
          lastElement?.focus()
        }
      } else {
        // Tab: moving forwards
        if (document.activeElement === lastElement) {
          e.preventDefault()
          firstElement?.focus()
        }
      }
    }

    document.addEventListener('keydown', handleEscape)
    document.addEventListener('keydown', handleTab)
    return () => {
      document.removeEventListener('keydown', handleEscape)
      document.removeEventListener('keydown', handleTab)
    }
  }, [isOpen, onClose])

  // Prevent body scroll when sheet is open
  useEffect(() => {
    if (isOpen) {
      document.body.style.overflow = 'hidden'
    } else {
      document.body.style.overflow = ''
    }
    return () => {
      document.body.style.overflow = ''
    }
  }, [isOpen])

  // Focus management: focus modal on open, restore focus on close
  useEffect(() => {
    if (isOpen) {
      // Save current focus
      previousFocusRef.current = document.activeElement as HTMLElement

      // Focus the first focusable element in the sheet
      setTimeout(() => {
        if (sheetRef.current) {
          const focusableElements = sheetRef.current.querySelectorAll<HTMLElement>(
            'button:not([disabled]), [href], input:not([disabled]), select:not([disabled]), textarea:not([disabled]), [tabindex]:not([tabindex="-1"])'
          )
          if (focusableElements.length > 0) {
            focusableElements[0].focus()
          }
        }
      }, 100) // Small delay to allow animation
    } else {
      // Restore focus when closing
      if (previousFocusRef.current) {
        previousFocusRef.current.focus()
      }
    }
  }, [isOpen])

  // Handle backdrop click
  const handleBackdropClick = useCallback(
    (e: React.MouseEvent) => {
      if (e.target === e.currentTarget) {
        onClose()
      }
    },
    [onClose]
  )

  if (!isOpen) return null

  return (
    <>
      {/* Backdrop - transparent to keep video visible */}
      <div
        className="fixed inset-0 z-[100]"
        onClick={handleBackdropClick}
        aria-hidden="true"
      />

      {/* Sheet - limited height to keep video visible above */}
      <div
        ref={sheetRef}
        className="fixed bottom-0 left-0 right-0 z-[101] bg-surface/95 backdrop-blur-sm rounded-t-2xl shadow-2xl transition-transform duration-300 ease-out border-t border-surface-elevated"
        style={{
          maxHeight: '45vh',
          transform: style.transform,
        }}
        role="dialog"
        aria-modal="true"
        aria-label={title}
        {...handlers}
      >
        {/* Drag Handle */}
        <div className="flex justify-center pt-3 pb-2 cursor-grab active:cursor-grabbing touch-none">
          <div className="w-12 h-1.5 bg-gray-500 rounded-full" />
        </div>

        {/* Header */}
        <div className="flex items-center justify-between px-4 pb-3 border-b border-surface-elevated">
          <h2 className="text-lg font-semibold">{title}</h2>
          <div className="flex items-center gap-3">
            {headerRight}
            <button
              type="button"
              onClick={onClose}
              className="p-2 hover:bg-surface-elevated rounded-full transition-colors"
              aria-label="Close"
            >
              <svg
                className="w-5 h-5"
                fill="none"
                stroke="currentColor"
                viewBox="0 0 24 24"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth={2}
                  d="M6 18L18 6M6 6l12 12"
                />
              </svg>
            </button>
          </div>
        </div>

        {/* Content */}
        <div
          ref={contentRef}
          className="overflow-y-auto overscroll-contain px-4 py-4"
          style={{ maxHeight: 'calc(45vh - 80px)' }}
        >
          {children}
        </div>
      </div>
    </>
  )
}
