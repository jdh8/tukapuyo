#include "AI.h"
#include "tt.h"
#include <cassert>
#include <algorithm>
#define HASH
unsigned int hash_hit;

int AI::thinkWrapper(const Field &self, const Field &enemy)
{
	int timeLimit = 999999;

	if (enemy.flag(RENSA))// 敵が連鎖中
	{
		// 連鎖が起こりきるまでの時間と思えばよい
		timeLimit = (enemy.chainMax() - enemy.chain()) * (CHAINTIME + FALLTIME);
	}
	LightField s(self);
	LightField e(enemy);

	// 世代を進める
	TT.newSearch();

	hash_hit = 0;

	// 思考開始
	think(s, e, 0, timeLimit);

	// ベストな手はbest_[0]にはいっている
	// それを操作に変換
	if (easy_ == 0)
	{
		operate_.generate(best_[0], self);
	}
	else if (easy_ == 1)
	{
		operate_.easyGenerate(best_[0], self);
	}
	else if (easy_ == 2)
	{
		operate_.veryEasyGenerate(best_[0], self);
	}
	
	return 0;
}


// 基本思考ルーチン::depthMax手読み
int AI::think(LightField &self, LightField &enemy, int depth, int timeLimit)
{
#ifdef HASH
	const int remain_depth = depth_max_ - depth;

	// 同一局面が発生するのは2手目以降しかありえない。
	if (depth >= 2)
	{
		const TTEntry *tte = TT.probe(self.key());
		// 局面が登録されていた
		if (tte)
		{
			// 局面表に登録されている局面が、現在の局面の残り深さ以上
			if (tte->remainDepth() >= remain_depth)
			{
				best_[depth] = tte->move();

				// 局面表のスコアは信用できるのでそのまま返す
				return tte->score();
			}
		}
	}
#endif

	if (depth >= depth_max_recieve_)
		return evalate(self, enemy, depth, timeLimit);
	else
	{	
		int score;
		int max = SCORE_LOSE - 1;
		int movenum;
		int con_prev[3];
		self.saveConnect(con_prev);
		Move move[22];// おける場所は最大で２２種類なので

		// 置く場所なし == 負け
		if ((movenum = self.generateMoves(move)) == 0)
			return SCORE_LOSE;
		
#if defined(DEBUG)
		LightField f = self;// debug
#endif

		self.nextPlus();

		for (int i = 0; i < movenum; i++)
		{
			// generateMovesでもとめた場所においてみる
			self.doMove<true>(move[i]);
			assert(self.key() == self.keyInit());
			// 設置にかかる時間を計算
			// 落下にかかる時間は、13 - 設置位置のyの小さいほう ちぎりなら、余計に時間がかかる
			int takeTime = (13 - std::min(toY(move[i].csq()), toY(move[i].psq()))) * FALLTIME + (move[i].isTigiri() ? TAKETIME_INCLUDETIGIRI : TAKETIME);

			if (self.flag(VANISH))// 消えるなら
			{
				score = evalVanish(self, enemy, depth, timeLimit) - takeTime;
				self.clearFlag(VANISH);
			}
			else// 消えないとき
			{
				if (timeLimit < takeTime)
				{
					// 致死量振るときに発火できなければ負け。
					if (enemy.scoreMax() >= 30 * RATE || enemy.scoreMax() >= self.deadLine() * RATE) 
						score = SCORE_LOSE;
					else 
						score = think(self, enemy, depth + 1, timeLimit - takeTime) - takeTime;
				}
				else
					score = think(self, enemy, depth + 1, timeLimit - takeTime) - takeTime;
			}

			self.undoMove(move[i], con_prev);

			if (max < score)
			{
				max = score;
				best_[depth] = move[i];// もっとも評価の高い手を選ぶ
			}

#if defined(DEBUG)
			assert(self == f); // debug::きちんともとの局面に戻せているか
#endif
		}

		self.nextMinus();

#ifdef HASH
		// この探索では後ろ二つの引数は使わない
		TT.store(self.key(), max, remain_depth, best_[depth], BOUND_EXACT, 0, 0, 0);
#endif
		return max;
	}
}

// 連鎖が起こらないときの評価関数
int AI3Connect::evalNoVanish(const LightField &self, const LightField &enemy, int depth, int timeLimit) const 
{
	eval_called++;
	int score = 0;

	if (timeLimit < 900000 && timeLimit > 0 && enemy.scoreMax() >= self.deadLine())
		score -= 20000 / timeLimit;

	score += self.connectBonus(-1, 0, 1);
	score += self.positionBonus();
	
	return score;
}

// 連鎖が起こるときの評価関数
int AI3Connect::evalVanish(LightField self, const LightField &enemy, int depth, int timeLimit)
{
	eval_called++;
	int score = 0;

	while(self.deleteMin());

	if (self.isDeath())
		return -999999;

	if (timeLimit - self.chain() * CHAINTIME <= 0)
	{
		if (enemy.scoreMax() - self.score() > self.deadLine() * (int)RATE)// 致死量を返しきれないなら負け
			score = -100000 * (depth_max_recieve_ - depth);
	}

	if (self.isEmpty())// 全けし
		score += RATE * 30;

	if (self.score() < RATE * 30)// 1列振らない消し方はマイナス評価
		score -= RATE * 29;

	score += self.score() - enemy.scoreMax();

	if (timeLimit < 900000 && timeLimit > 0 && enemy.scoreMax() >= self.deadLine() * (int)RATE)
		score -= 20000 / timeLimit;

	score += self.connectBonus(-6, 3, 6);
	score += self.positionBonus();
	
	return score;
}

int AI3Connect2::evalNoVanish(const LightField &self, const LightField &enemy, int depth, int timeLimit) const 
{
	eval_called++;
	int score = 0;

	if (timeLimit < 900000 && timeLimit > 0 && enemy.scoreMax() >= self.deadLine() * (int)RATE)
		score -= 20000 / timeLimit;

	score += self.connectBonus(-6, 3, 6) / 2;
	//score += self.positionBonus();
	for (int x = 1; x <= 6; x++)
	{
		for (int y = 7; y < self.upper(x); y++)
		{ 
			if (x != 3)
			{
				if (!self.isEmpty(toSquare(x, y)))
					score -= RATE * y / abs(3-x);
			}
			else
			{
				if (!self.isEmpty(toSquare(x, y)))
					score -= RATE * y * 3;
			}
		}
	}
	return score;
}

int AI3Connect2::evalVanish(LightField self, const LightField &enemy, int depth, int timeLimit)
{
	eval_called++;
	int score = 0;

	const int self_num = self.getPuyoNum();
	while (self.deleteMin());

	if (self.isDeath())
		return SCORE_LOSE;

	if (timeLimit < 0)
	{
		if (enemy.scoreMax() - self.score() > self.deadLine() * (int)RATE)// 致死量を返しきれないなら負
			return -999999;
		else
			score += self.score() - enemy.scoreMax();
	}

	if (self.isEmpty())// 全けし
	{
		score += RATE * 30;
	}

	//if (self.score() < RATE * 20)// 1列振らない消し方はマイナス評価
	if (self.score() < RATE * 30)// 1列振らない消し方はマイナス評価
		score -= RATE * 29;
	else
		score += self.score();
		//score += s;

	score += self.connectBonus(-6, 3, 6) / 2;
	//score += self.positionBonus();
	for (int x = 1; x <= 6; x++)
	{
		for (int y = 7; y < self.upper(x); y++)
		{ 
			if (x != 3)
			{
				if (!self.isEmpty(toSquare(x, y)))
					score -= RATE * y / abs(3-x);
			}
			else
			{
				if (!self.isEmpty(toSquare(x, y)))
					score -= RATE * y * 3;
			}
		}
	}
	return score;
}



