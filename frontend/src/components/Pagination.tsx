interface PaginationProps {
  offset: number;
  limit: number;
  total: number;
  onPageChange: (newOffset: number) => void;
  context?: string; // Optional context like "on Jan 15, 2025"
}

export function Pagination({ offset, limit, total, onPageChange, context }: PaginationProps) {
  const currentPage = Math.floor(offset / limit) + 1;
  const totalPages = Math.ceil(total / limit);
  const displayStart = total === 0 ? 0 : offset + 1;
  const displayEnd = Math.min(offset + limit, total);

  const canGoPrevious = offset > 0;
  const canGoNext = offset + limit < total;

  return (
    <div className="flex items-center justify-between py-3 px-1">
      <span className="text-sm text-gray-400">
        Displaying {displayStart}-{displayEnd} of {total}
        {context && <span className="text-gray-500"> {context}</span>}
      </span>
      {totalPages > 1 && (
        <div className="flex gap-2 items-center">
          <button
            onClick={() => onPageChange(Math.max(0, offset - limit))}
            disabled={!canGoPrevious}
            className="px-3 py-1 bg-surface-elevated hover:bg-surface rounded disabled:opacity-50 disabled:cursor-not-allowed text-sm transition-colors"
            aria-label="Previous page"
          >
            ◀ Previous
          </button>
          <span className="px-3 py-1 text-sm text-gray-400">
            Page {currentPage} of {totalPages}
          </span>
          <button
            onClick={() => onPageChange(offset + limit)}
            disabled={!canGoNext}
            className="px-3 py-1 bg-surface-elevated hover:bg-surface rounded disabled:opacity-50 disabled:cursor-not-allowed text-sm transition-colors"
            aria-label="Next page"
          >
            Next ▶
          </button>
        </div>
      )}
    </div>
  );
}
