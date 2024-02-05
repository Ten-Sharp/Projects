package assignment_1;

import java.util.Arrays;
import java.util.Scanner;

public class Main {

	public static Scanner input = new Scanner(System.in);
	public static int N;
	
	public static void main(String[] args) {
		double temp = 1000;
		double decay = 0.98;
		
		System.out.print("Please select the N value: ");
		N = input.nextInt();
		
		long startTime = System.nanoTime();
		
		int[][] board = new int[N][N];		
		
		for(int i = 0; i<N; i++) {
			Arrays.fill(board[i], 0);
			int z = (int) (Math.random()*N);
			board[i][z] = 1;
		}
		
		printBoard(board);
		System.out.println("\n");
		
		do {
			int[][] newState = neighbour(board);
			int cost = cost(board);
			int newCost = cost(newState);
			
			if(newCost < cost) {
				board = newState;
				temp = temp * decay;
			}
			else if(newCost >= cost) {
				double prob = Math.exp((-1)*((newCost - cost)/temp));
				if(Math.random() <= prob) {
					board = newState;
				}
				temp = temp * decay;
			}
			//resets temp if plateaus or stalls
			if(temp< 1.0E-300) temp = 1000;
		}while(cost(board) != 0);
		
		long endTime = System.nanoTime();
		
		printBoard(board);
		System.out.println("\n Runtime: "+(double)((double)(endTime - startTime)/1000000));
	}
	
	private static int conflictNum(int[][] board,int row, int col) {
		int conflicts = 0;
		//scans column
		for(int x=0;x<N;x++) {
			conflicts += board[x][col];
		}
		conflicts -= 1;
		//scans diagonals
		for(int i =0;i<N;i++) {
			for(int j=0;j<N;j++) {
				if(i!=row && board[i][j] == 1 && ((i-j == row-col)||(i+j == row+col))) {
					conflicts += 1;
				}
			}
		}
		return conflicts;
	}
	
	private static int cost(int[][] board) {
		int conflicts = 0;
		for(int x =0;x<N;x++) {
			for(int y=0;y<N;y++) {
				if(board[x][y] == 1) {
					conflicts += conflictNum(board,x,y);
				}
			}
		}
		return conflicts;
	}
	
	private static int[][] neighbour(int[][] board){
		int[][] newBoard = new int[N][N];
		//deep copy of board
		for(int x=0;x<N;x++) {
			for(int y=0;y<N;y++) {
				newBoard[x][y] = board[x][y];
			}
		}
		
		int row = (int)(Math.random()*N);
		int currentPos = getCol(board,row);
		newBoard[row][currentPos] = 0;
		int pos = (int)Math.random()*N;
		while(pos == currentPos) {
			pos=(int)(Math.random()*N); 
		}
		newBoard[row][pos] = 1;
		return newBoard;
	}
	
	private static int getCol(int[][] board, int row) {
		int col = 0;
		for(int i = 0;i<N;i++) {
			if(board[row][i] == 1) {
				col = i;
			}
		}
		return col;
	}
	
	private static void printBoard(int[][] board) {
		for(int i=0;i<N;i++) {
			for(int j=0;j<N;j++) {
				System.out.print(board[i][j] + "  ");
			}
			System.out.println("");
		}
	}
}
